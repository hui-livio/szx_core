import { test, expect, type Page } from '@playwright/test'
import { chooseOption } from './select'

async function setup(page: Page, total = 205) {
  const reads: URL[] = []
  let failNext = false
  let release: (() => void) | undefined
  let held = false
  const user = {
    id: 'pagination-admin',
    login_identifier: 'admin',
    display_name: '分页管理员',
    role: 'super_admin',
    status: 'active',
    version: 1,
    authz_version: 1,
    force_password_change: false,
    password_change_recommended: false,
    created_at: 0,
    updated_at: 0,
    last_login_at: null,
  }
  await page.route('**/api/v1/**', async (route) => {
    const url = new URL(route.request().url())
    const path = url.pathname
    const reply = (data: unknown) =>
      route.fulfill({ json: { data, request_id: 'pagination-request' } })
    if (path.endsWith('/auth/login'))
      return reply({
        user,
        access_token: 'pagination-access',
        csrf_token: 'pagination-csrf',
        expires_at: Date.now() + 900000,
        expires_in: 900,
        authz_version: 1,
        token_type: 'Bearer',
      })
    if (path.endsWith('/client/me/permissions'))
      return reply({
        user_id: user.id,
        role: user.role,
        capabilities: ['user.read', 'audit.read', 'behavior.read'],
        authz_version: 1,
        status: 'active',
        force_password_change: false,
      })
    if (
      path.endsWith('/admin/users') ||
      path.endsWith('/admin/audit-logs') ||
      path.endsWith('/admin/behavior-events')
    ) {
      reads.push(url)
      if (held) {
        held = false
        await new Promise<void>((resolve) => {
          release = resolve
        })
      }
      if (failNext) {
        failNext = false
        return route.fulfill({ status: 503, json: { code: 2299 } })
      }
      const current = Number(url.searchParams.get('page') ?? 1)
      const size = Number(url.searchParams.get('page_size') ?? 20)
      const offset = (current - 1) * size
      const items = Array.from({ length: Math.max(0, Math.min(size, total - offset)) }, (_, i) =>
        path.endsWith('/admin/users')
          ? {
              ...user,
              id: `operator-${offset + i}`,
              login_identifier: `operator-${offset + i}`,
              display_name: `操作员 ${offset + i}`,
              role: 'operator',
            }
          : {
              id: `event-${offset + i}`,
              event_name: path.endsWith('/admin/audit-logs') ? 'auth.login' : 'report.view',
              result: 'success',
              actor_user_id: user.id,
              actor_login_identifier: user.login_identifier,
              user_id: user.id,
              login_identifier: user.login_identifier,
              target_user_id: '',
              request_id: 'log-record',
              details: {},
              created_at: Date.now(),
            },
      )
      return reply({ items, total, page: current, page_size: size })
    }
    return reply({ items: [], total: 0, page: 1, page_size: 20 })
  })
  await page.goto('/login')
  await page.locator('input[name="identifier"]').fill('admin')
  await page.locator('input[name="password"]').fill('TestPassword123')
  await page.locator('.login-submit').click()
  await expect(page).toHaveURL('/home')
  return {
    reads,
    fail: () => {
      failNext = true
    },
    hold: () => {
      held = true
    },
    release: () => release?.(),
    latest: (path: string) => reads.filter((url) => url.pathname.endsWith(path)).at(-1)!,
  }
}

async function chooseSize(page: Page, size: number) {
  await chooseOption(
    page,
    page.getByRole('combobox', { name: '每页条数', exact: true }),
    String(size),
  )
  await expect(page.locator('.admin-table tbody tr')).toHaveCount(size)
}

async function customSize(page: Page, size: string, enter = false) {
  const dropdown = page.getByRole('combobox', { name: '每页条数', exact: true })
  await chooseOption(page, dropdown, '自定义条数')
  const input = page.getByRole('spinbutton', { name: '自定义条数', exact: true })
  await input.fill(size)
  if (enter) await input.press('Enter')
  else await page.locator('.pagination').getByRole('button', { name: '应用', exact: true }).click()
}

test('preset and arbitrary page sizes reset users to page one and preserve applied filters', async ({
  page,
}) => {
  const state = await setup(page)
  await page.locator('.sidebar a[href="/users"]').click()
  await expect(page.locator('.admin-table tbody tr')).toHaveCount(20)
  await page.getByRole('button', { name: '下一页', exact: true }).click()
  await expect(page.locator('.p-paginator-current')).toHaveText('2 / 11')
  await chooseSize(page, 50)
  expect(state.latest('/admin/users').searchParams.get('page')).toBe('1')
  expect(state.latest('/admin/users').searchParams.get('page_size')).toBe('50')
  await expect(page.locator('.p-paginator-current')).toHaveText('1 / 5')
  const search = page.getByRole('searchbox', { name: '搜索用户', exact: true })
  await search.fill('applied')
  await search.press('Enter')
  await expect.poll(() => state.latest('/admin/users').searchParams.get('search')).toBe('applied')
  await search.fill('unsubmitted')
  await customSize(page, '37', true)
  await expect(page.locator('.admin-table tbody tr')).toHaveCount(37)
  const request = state.latest('/admin/users')
  expect(request.searchParams.get('page')).toBe('1')
  expect(request.searchParams.get('page_size')).toBe('37')
  expect(request.searchParams.get('search')).toBe('applied')
  await expect(page.locator('.p-paginator-current')).toHaveText('1 / 6')
  await page.getByRole('button', { name: '下一页', exact: true }).click()
  await expect.poll(() => state.latest('/admin/users').searchParams.get('page')).toBe('2')
  expect(state.latest('/admin/users').searchParams.get('page_size')).toBe('37')
})

test('invalid custom sizes never request data; selected size is disabled while busy and survives failure retry', async ({
  page,
}) => {
  const state = await setup(page)
  await page.locator('.sidebar a[href="/users"]').click()
  await expect(page.locator('.admin-table tbody tr')).toHaveCount(20)
  await chooseOption(
    page,
    page.getByRole('combobox', { name: '每页条数', exact: true }),
    '自定义条数',
  )
  const input = page.getByRole('spinbutton', { name: '自定义条数', exact: true })
  const apply = page.locator('.pagination').getByRole('button', { name: '应用', exact: true })
  const count = state.reads.length
  for (const value of ['0', '101', '1.5', '-3', '']) {
    await input.fill(value)
    await expect(apply).toBeDisabled()
    await expect(input).toHaveAttribute('aria-invalid', 'true')
    await input.press('Enter')
  }
  expect(state.reads).toHaveLength(count)
  await input.fill('37')
  await expect(input).toHaveAttribute('aria-invalid', 'false')
  state.hold()
  await apply.click()
  await expect(input).toBeDisabled()
  await expect(page.getByRole('combobox', { name: '每页条数', exact: true })).toBeDisabled()
  await expect(page.getByRole('button', { name: '下一页', exact: true })).toBeDisabled()
  await expect.poll(() => state.latest('/admin/users').searchParams.get('page_size')).toBe('37')
  state.release()
  await expect(page.locator('.admin-table tbody tr')).toHaveCount(37)
  state.fail()
  await input.fill('73')
  await apply.click()
  await expect(page.getByRole('alert')).toBeVisible()
  await expect(page.locator('.p-paginator-current')).toContainText('1 /')
  await page.getByRole('button', { name: '刷新列表', exact: true }).click()
  await expect(page.locator('.admin-table tbody tr')).toHaveCount(73)
  expect(state.latest('/admin/users').searchParams.get('page_size')).toBe('73')
  expect(state.latest('/admin/users').searchParams.get('page')).toBe('1')
})

test('audit and behavior pagination keep independent sizes across tab switches', async ({
  page,
}) => {
  const state = await setup(page)
  await page.locator('.sidebar a[href="/logs"]').click()
  await expect(page.locator('.admin-table tbody tr')).toHaveCount(20)
  await chooseSize(page, 50)
  expect(state.latest('/admin/audit-logs').searchParams.get('page_size')).toBe('50')
  await page.getByRole('tab', { name: '用户行为', exact: true }).click()
  await expect(page.locator('.admin-table tbody tr')).toHaveCount(20)
  await customSize(page, '37')
  await expect(page.locator('.admin-table tbody tr')).toHaveCount(37)
  expect(state.latest('/admin/behavior-events').searchParams.get('page_size')).toBe('37')
  await page.getByRole('tab', { name: '安全审计', exact: true }).click()
  await expect(page.getByRole('combobox', { name: '每页条数', exact: true })).toHaveText('50')
  await page.getByRole('tab', { name: '用户行为', exact: true }).click()
  await expect(page.getByRole('spinbutton', { name: '自定义条数', exact: true })).toHaveValue('37')
})

for (const width of [320, 390, 1440]) {
  test(`page size input stays usable and table scroll remains bounded at ${width}px`, async ({
    page,
  }) => {
    await page.setViewportSize({ width, height: width < 500 ? 844 : 900 })
    await setup(page)
    await page.locator('.sidebar a[href="/users"]').click()
    await customSize(page, '37')
    await expect(page.locator('.admin-table tbody tr')).toHaveCount(37)
    const footer = page.locator('.pagination')
    const bounds = await footer.boundingBox()
    expect(bounds!.x).toBeGreaterThanOrEqual(0)
    expect(bounds!.x + bounds!.width).toBeLessThanOrEqual(width)
    const input = await page
      .getByRole('spinbutton', { name: '自定义条数', exact: true })
      .boundingBox()
    expect(input!.x).toBeGreaterThanOrEqual(0)
    expect(input!.x + input!.width).toBeLessThanOrEqual(width)
    const scroll = page.locator('.table-scroll')
    expect(await scroll.evaluate((e) => e.clientHeight)).toBeGreaterThan(50)
    const position = bounds!.y
    await scroll.evaluate((e) => {
      e.scrollTop = 500
    })
    expect((await footer.boundingBox())!.y).toBe(position)
    await page.screenshot({ path: `test-results/page-size-${width}.png` })
  })
}

test('empty results still allow changing size without enabling page navigation', async ({
  page,
}) => {
  const state = await setup(page, 0)
  await page.locator('.sidebar a[href="/users"]').click()
  await expect(page.locator('.p-paginator-current')).toHaveText('1 / 1')
  await expect(page.getByRole('combobox', { name: '每页条数', exact: true })).toBeEnabled()
  await chooseOption(page, page.getByRole('combobox', { name: '每页条数', exact: true }), '100')
  await expect.poll(() => state.latest('/admin/users').searchParams.get('page_size')).toBe('100')
  await expect(page.locator('.p-paginator-current')).toHaveText('1 / 1')
  await expect(page.getByRole('button', { name: '上一页', exact: true })).toBeDisabled()
  await expect(page.getByRole('button', { name: '下一页', exact: true })).toBeDisabled()
})

test('first and maximum allowed sizes paginate correctly and work in English', async ({ page }) => {
  const state = await setup(page)
  await page.locator('.sidebar a[href="/users"]').click()
  await customSize(page, '1')
  await expect(page.locator('.admin-table tbody tr')).toHaveCount(1)
  await expect(page.locator('.p-paginator-current')).toHaveText('1 / 205')
  await chooseSize(page, 100)
  await page.getByRole('button', { name: '下一页', exact: true }).click()
  await expect(page.locator('.p-paginator-current')).toHaveText('2 / 3')
  await page.getByRole('button', { name: '下一页', exact: true }).click()
  await expect(page.locator('.admin-table tbody tr')).toHaveCount(5)
  await expect(page.locator('.p-paginator-current')).toHaveText('3 / 3')
  await expect(page.getByRole('button', { name: '下一页', exact: true })).toBeDisabled()
  await chooseOption(page, page.locator('.language-switcher').getByRole('combobox'), 'English')
  await chooseOption(
    page,
    page.getByRole('combobox', { name: 'Rows per page', exact: true }),
    'Custom page size',
  )
  await page.getByRole('spinbutton', { name: 'Custom page size', exact: true }).fill('37')
  await page.locator('.pagination').getByRole('button', { name: 'Apply', exact: true }).click()
  await expect(page.locator('.admin-table tbody tr')).toHaveCount(37)
  expect(state.latest('/admin/users').searchParams.get('page')).toBe('1')
  await expect(page.locator('.p-paginator-current')).toHaveText('1 / 6')
})
