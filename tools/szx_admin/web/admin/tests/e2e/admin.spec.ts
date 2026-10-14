import { chooseOption } from './select'
import { test, expect, type Page } from '@playwright/test'
import { createHash } from 'node:crypto'
import type { Role, User } from '../../src/api/types'

const baseUser: User = {
  id: '10000000-0000-4000-8000-000000000002',
  login_identifier: 'alice',
  display_name: 'Alice',
  role: 'operator',
  status: 'active',
  version: 3,
  authz_version: 1,
  force_password_change: false,
  password_change_recommended: false,
  created_at: 1789636500000,
  updated_at: 1789636500000,
  last_login_at: null,
}
async function mockApi(
  page: Page,
  role: Role = 'super_admin',
  force = false,
  recommended = false,
  self = false,
) {
  const actor = {
    ...baseUser,
    id: '10000000-0000-4000-8000-000000000001',
    login_identifier: 'admin',
    display_name: '系统管理员',
    role,
    force_password_change: force,
    password_change_recommended: recommended,
  }
  const session = {
    user: actor,
    access_token: 'test-access',
    csrf_token: 'test-csrf',
    expires_at: Date.now() + 900_000,
    expires_in: 900,
    authz_version: 1,
    token_type: 'Bearer',
  }
  const listedUser = self ? actor : baseUser
  const writes: { path: string; body: unknown; version?: string }[] = []
  await page.route('**/api/v1/**', async (route) => {
    const req = route.request()
    const path = new URL(req.url()).pathname.replace('/api/v1', '')
    const body = req.postData() ? req.postDataJSON() : undefined
    if (req.method() !== 'GET') writes.push({ path, body, version: req.headers()['if-match'] })
    const reply = (data: unknown, status = 200) =>
      route.fulfill({ status, json: { data, request_id: 'test-request' } })
    if (path === '/auth/login' || path === '/auth/refresh') return reply(session)
    if (path === '/auth/logout') return route.fulfill({ status: 204 })
    if (path === '/auth/change-password') return reply({ reauthentication_required: true })
    if (path === '/client/me/permissions')
      return reply({
        role,
        capabilities: role === 'operator' ? ['business.read'] : ['user.read', 'audit.read'],
        user_id: actor.id,
        authz_version: 1,
        status: 'active',
        force_password_change: force,
      })
    if (path === '/admin/users' && req.method() === 'GET')
      return reply({ items: [listedUser], total: 1, page: 1, page_size: 20 })
    if (path === '/admin/users' && req.method() === 'POST')
      return reply(
        {
          user: { ...baseUser, ...body },
          temporary_credential: body.password_hash ? null : 'OnceOnlyCredential123',
          temporary_credential_expires_at: body.password_hash ? null : Date.now() + 86400000,
        },
        201,
      )
    if (path === `/admin/users/${listedUser.id}` && req.method() === 'GET') return reply(listedUser)
    if (path.endsWith('/reset-password'))
      return reply({
        user: { ...listedUser, authz_version: 2, version: 4 },
        temporary_credential: body?.password_hash ? null : 'SelfResetCredential123',
        temporary_credential_expires_at: body?.password_hash ? null : Date.now() + 86400000,
      })
    if (path.startsWith('/admin/users/')) return reply({ ...baseUser, ...body, version: 4 })
    if (path === '/admin/audit-logs')
      return reply({
        items: [
          {
            id: 'log',
            event_name: 'auth.login',
            result: 'success',
            actor_user_id: actor.id,
            actor_login_identifier: actor.login_identifier,
            target_user_id: actor.id,
            request_id: 'audit-id',
            details: { password_hash: 'should-not-appear', source: 'web' },
            created_at: Date.now(),
          },
        ],
        total: 1,
        page: 1,
        page_size: 20,
      })
    if (path === '/admin/behavior-events')
      return reply({
        items: [
          {
            id: 'behavior-log',
            user_id: actor.id,
            login_identifier: actor.login_identifier,
            event_name: 'report.view',
            result: 'success',
            request_id: 'behavior-id',
            details: { source: 'web' },
            created_at: Date.now(),
          },
        ],
        total: 1,
        page: 1,
        page_size: 20,
      })
    return route.fulfill({ status: 404, json: { code: 2205 } })
  })
  return writes
}
async function login(page: Page) {
  await page.goto('/login')
  await page.locator('input[name="identifier"]').fill('admin')
  await page.getByLabel('密码', { exact: true }).fill('TestPassword123')
  await page.getByRole('button', { name: '登录控制台' }).click()
}
test('admin creates a user, clears one-time credentials and submits versioned edits', async ({
  page,
}) => {
  const writes = await mockApi(page)
  await login(page)
  await expect(page).toHaveURL('/users')
  expect(writes[0]?.body).toMatchObject({
    login_identifier: 'admin',
    password_hash: expect.stringMatching(/^[a-f0-9]{64}$/),
  })
  await page.getByRole('button', { name: '创建用户' }).click()
  await page.getByLabel('登录标识', { exact: true }).fill('bob')
  await page.getByLabel('展示名', { exact: true }).fill('Bob')
  await page.getByRole('button', { name: '确认', exact: true }).click()
  await expect(page.getByTestId('temporary-credential')).toHaveText('OnceOnlyCredential123')
  expect(writes.find((write) => write.path === '/admin/users')?.body).not.toHaveProperty(
    'password_hash',
  )
  await page.getByRole('button', { name: '已妥善记录，关闭' }).click()
  await expect(page.getByTestId('temporary-credential')).toHaveCount(0)
  expect(await page.evaluate(() => [localStorage.length, sessionStorage.length])).toEqual([0, 0])
  await page.getByRole('button', { name: '编辑', exact: true }).click()
  await expect(page.getByRole('button', { name: '确认', exact: true })).toBeEnabled()
  await page.getByLabel('展示名', { exact: true }).fill('Alice Updated')
  await page.getByRole('button', { name: '确认', exact: true }).click()
  await expect(page.getByRole('dialog')).toHaveCount(0)
  expect(writes.find((write) => write.path.endsWith(baseUser.id))).toMatchObject({
    version: '3',
    body: { display_name: 'Alice Updated' },
  })
  await page.screenshot({ path: 'test-results/users-desktop.png', fullPage: true })
})
test('creation validates a chosen password, submits only its digest and clears it', async ({
  page,
}) => {
  const writes = await mockApi(page)
  await login(page)
  await page.getByRole('button', { name: '创建用户' }).click()
  await page.getByLabel('登录标识', { exact: true }).fill('chosen')
  await page.getByLabel('展示名', { exact: true }).fill('指定密码')
  const password = page.getByLabel('初始密码', { exact: true })
  const confirmation = page.getByLabel('确认初始密码', { exact: true })
  await password.fill('12345')
  await confirmation.fill('12345')
  await page.getByRole('button', { name: '确认', exact: true }).click()
  await expect(page.getByRole('alert')).toContainText('至少 6 个字符')
  const secret = '  自定义Password123  '
  await password.fill(secret)
  await confirmation.fill('different')
  await page.getByRole('button', { name: '确认', exact: true }).click()
  await expect(page.getByRole('alert')).toContainText('不一致')
  expect(writes.filter((write) => write.path === '/admin/users')).toHaveLength(0)
  await confirmation.fill(secret)
  await page.getByRole('button', { name: '确认', exact: true }).click()
  await expect(page.getByRole('dialog')).toHaveCount(0)
  expect(writes.find((write) => write.path === '/admin/users')?.body).toEqual({
    login_identifier: 'chosen',
    display_name: '指定密码',
    role: 'operator',
    password_hash: createHash('sha256').update(secret).digest('hex'),
  })
  await expect(page.getByTestId('temporary-credential')).toHaveCount(0)
  await page.getByRole('button', { name: '创建用户' }).click()
  await expect(password).toHaveValue('')
  await expect(confirmation).toHaveValue('')
  await password.fill(secret)
  await confirmation.fill(secret)
  await page.getByRole('button', { name: '取消', exact: true }).click()
  await page.getByRole('button', { name: '创建用户' }).click()
  await expect(password).toHaveValue('')
  await expect(confirmation).toHaveValue('')
  expect(await page.evaluate(() => [localStorage.length, sessionStorage.length])).toEqual([0, 0])
})

for (const role of ['operator', 'engineer'] as const) {
  test(`${role} accepts a simple initial password after switching from admin`, async ({ page }) => {
    const writes = await mockApi(page)
    await login(page)
    await page.getByRole('button', { name: '创建用户' }).click()
    await page.getByLabel('登录标识', { exact: true }).fill('simple')
    await page.getByLabel('展示名', { exact: true }).fill('Simple')
    const select = page.getByRole('dialog').getByLabel('角色', { exact: true })
    await chooseOption(page, select, '管理员')
    await expect(page.locator('#initial-password-rule')).toContainText('至少 8 个字符')
    await page.getByLabel('初始密码', { exact: true }).fill('123456')
    await page.getByLabel('确认初始密码', { exact: true }).fill('123456')
    await page.getByRole('button', { name: '确认', exact: true }).click()
    await expect(page.getByRole('alert')).toContainText('至少 8 个字符')
    expect(writes.filter((write) => write.path === '/admin/users')).toHaveLength(0)
    await chooseOption(page, select, role === 'operator' ? '操作员' : '工程师')
    await expect(page.locator('#initial-password-rule')).toContainText('至少 6 个字符')
    await page.getByRole('button', { name: '确认', exact: true }).click()
    await expect(page.getByRole('dialog')).toHaveCount(0)
    expect(writes.find((write) => write.path === '/admin/users')?.body).toMatchObject({
      role,
      password_hash: createHash('sha256').update('123456').digest('hex'),
    })
  })
  test(`${role} can change to a six-digit password`, async ({ page }) => {
    const writes = await mockApi(page, role, false, true)
    await login(page)
    await expect(page).toHaveURL('/change-password')
    await expect(page.locator('#password-rule')).toContainText('至少 6 个字符')
    await page.getByLabel('当前密码', { exact: true }).fill('TestPassword123')
    await page.getByLabel('新密码', { exact: true }).fill('123456')
    await page.getByLabel('确认新密码', { exact: true }).fill('123456')
    await page.getByRole('button', { name: '修改密码', exact: true }).click()
    await expect(page).toHaveURL('/login')
    expect(writes.find((write) => write.path === '/auth/change-password')?.body).toMatchObject({
      new_password_hash: createHash('sha256').update('123456').digest('hex'),
    })
  })
}

test('forced password flow cannot be skipped and hashes both passwords', async ({ page }) => {
  const writes = await mockApi(page, 'super_admin', true)
  await login(page)
  await expect(page).toHaveURL('/change-password')
  await expect(page.getByRole('button', { name: /跳过/ })).toHaveCount(0)
  await page.getByLabel('当前密码', { exact: true }).fill('TestPassword123')
  await page.getByLabel('新密码', { exact: true }).fill('Abcd1234')
  await page.getByLabel('确认新密码', { exact: true }).fill('Abcd1234')
  await page.getByRole('button', { name: '修改密码', exact: true }).click()
  await expect(page).toHaveURL('/login')
  expect(writes.find((write) => write.path === '/auth/change-password')).toMatchObject({
    version: '3',
    body: {
      old_password_hash: expect.stringMatching(/^[a-f0-9]{64}$/),
      new_password_hash: expect.stringMatching(/^[a-f0-9]{64}$/),
    },
  })
})
test('operator can skip recommendation and is blocked from admin routes', async ({ page }) => {
  await mockApi(page, 'operator', false, true)
  await login(page)
  await page.getByRole('button', { name: '跳过，稍后修改' }).click()
  await expect(page).toHaveURL('/home')
  await expect(page.getByRole('link', { name: '用户管理' })).toHaveCount(0)
  await page.evaluate(() => {
    history.pushState({}, '', '/users')
    window.dispatchEvent(new PopStateEvent('popstate'))
  })
  await expect(page).toHaveURL('/home')
})
test('admin role options are restricted and audit details are redacted', async ({ page }) => {
  await mockApi(page, 'admin')
  await login(page)
  await expect(page.getByRole('button', { name: '分配角色', exact: true })).toBeEnabled()
  await page.getByRole('button', { name: '创建用户' }).click()
  await page.getByRole('dialog').getByRole('combobox', { name: '角色', exact: true }).click()
  await expect(page.getByRole('option')).toHaveText(['操作员', '工程师'])
  await page.keyboard.press('Escape')
  await expect(page.getByRole('listbox')).toHaveCount(0)
  await page.keyboard.press('Escape')
  await expect(page.getByRole('dialog')).toHaveCount(0)
  await page.getByRole('link', { name: '审计日志' }).click()
  await expect(page.getByRole('columnheader', { name: '登录标识', exact: true })).toBeVisible()
  await expect(page.getByRole('cell', { name: 'admin', exact: true })).toBeVisible()
  await expect(page.getByRole('searchbox', { name: '搜索用户', exact: true })).toHaveAttribute(
    'placeholder',
    '搜索展示名或登录标识',
  )
  await page.getByRole('searchbox', { name: '搜索用户', exact: true }).fill(' 系统管理员 ')
  const searchRequest = page.waitForRequest((request) =>
    new URL(request.url()).searchParams.has('actor_search'),
  )
  await page.getByRole('button', { name: '查询日志' }).click()
  const searchUrl = new URL((await searchRequest).url())
  expect(searchUrl.searchParams.get('actor_search')).toBe('系统管理员')
  expect(searchUrl.searchParams.has('actor_user_id')).toBe(false)
  await page.getByRole('button', { name: '查看详情' }).click()
  await expect(page.getByRole('dialog').getByText('admin', { exact: true })).toBeVisible()
  await expect(page.getByRole('dialog')).toContainText('web')
  await expect(page.getByRole('dialog')).not.toContainText('should-not-appear')
})
test('audit event search sends trimmed keywords and clears the filter', async ({ page }) => {
  await mockApi(page)
  await login(page)
  await page.getByRole('link', { name: '审计日志' }).click()
  const input = page.getByRole('combobox', { name: '事件名称', exact: true })
  await expect(input).toHaveAttribute('maxlength', '128')
  await expect(input).not.toHaveAttribute('list')
  await expect(page.locator('datalist')).toHaveCount(0)
  await page.locator('.log-filters .p-select-dropdown').first().click()
  await expect(page.getByRole('option', { name: '登录成功', exact: true })).toContainText(
    'auth.login.success',
  )
  await input.press('Escape')
  for (const term of ['login', 'auth.login', '登录', '']) {
    await input.fill(` ${term} `)
    const request = page.waitForRequest((request) =>
      new URL(request.url()).pathname.endsWith('/admin/audit-logs'),
    )
    await page.getByRole('button', { name: '查询日志' }).click()
    const params = new URL((await request).url()).searchParams
    expect(params.get('event_search')).toBe(term || null)
    expect(params.has('event_name')).toBe(false)
    expect(params.get('page')).toBe('1')
    await expect(page.getByRole('button', { name: '查询日志' })).toBeEnabled()
  }
  await input.fill('登录成功')
  await page.getByRole('searchbox', { name: '搜索用户', exact: true }).fill('admin')
  await page.getByLabel('开始时间（本地）').fill('2026-09-01 00:00:00')
  const resetRequest = page.waitForRequest((request) =>
    new URL(request.url()).pathname.endsWith('/admin/audit-logs'),
  )
  await page.getByRole('button', { name: '重置筛选' }).click()
  const resetParams = new URL((await resetRequest).url()).searchParams
  expect([...resetParams.keys()].sort()).toEqual(['page', 'page_size'])
  await expect(input).toHaveValue('')
  await expect(page.getByRole('searchbox', { name: '搜索用户', exact: true })).toHaveValue('')
  await expect(page.getByLabel('开始时间（本地）')).toHaveValue('')
})

test('behavior tab uses the split endpoint and keeps independent filters', async ({ page }) => {
  await mockApi(page)
  await login(page)
  await page.getByRole('link', { name: '审计日志' }).click()
  await page.getByRole('tab', { name: '用户行为' }).click()
  await expect(
    page.getByRole('cell', { name: /report\.view/ }).getByText('report.view'),
  ).toBeVisible()
  await page.getByRole('searchbox', { name: '搜索用户', exact: true }).fill(' alice ')
  const request = page.waitForRequest((candidate) =>
    new URL(candidate.url()).pathname.endsWith('/admin/behavior-events'),
  )
  await page.getByRole('button', { name: '查询事件' }).click()
  const params = new URL((await request).url()).searchParams
  expect(params.get('user_search')).toBe('alice')
  expect(params.has('actor_search')).toBe(false)
  await page.getByRole('tab', { name: '安全审计' }).click()
  await expect(page.getByRole('searchbox', { name: '搜索用户', exact: true })).toHaveValue('')
})

test('login remains usable at mobile width', async ({ page }) => {
  await page.setViewportSize({ width: 390, height: 844 })
  await page.goto('/login')
  await expect(page.getByRole('button', { name: '登录控制台' })).toBeVisible()
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true)
  await page.screenshot({ path: 'test-results/login-mobile.png', fullPage: true })
})

test('reset supports a simple chosen password and clears the draft', async ({ page }) => {
  const writes = await mockApi(page)
  await login(page)
  await page.getByRole('button', { name: '重置密码', exact: true }).click()
  const confirm = page.getByRole('button', { name: '确认', exact: true })
  await expect(confirm).toBeEnabled()
  await expect(page.locator('#initial-password-rule')).toContainText('至少 6 个字符')
  await page.getByLabel('新密码', { exact: true }).fill('12345')
  await page.getByLabel('确认新密码', { exact: true }).fill('12345')
  await confirm.click()
  await expect(page.getByRole('alert').filter({ hasText: '至少 6 个字符' })).toBeVisible()
  await page.getByLabel('新密码', { exact: true }).fill('123456')
  await page.getByLabel('确认新密码', { exact: true }).fill('123456')
  await confirm.click()
  await expect(page.getByRole('dialog')).toHaveCount(0)
  expect(writes.find((write) => write.path.endsWith('/reset-password'))).toMatchObject({
    version: '3',
    body: { password_hash: createHash('sha256').update('123456').digest('hex') },
  })
  await page.getByRole('button', { name: '重置密码', exact: true }).click()
  await expect(page.getByLabel('新密码', { exact: true })).toHaveValue('')
  await expect(page.getByLabel('确认新密码', { exact: true })).toHaveValue('')
})

test('self role and disable actions are unavailable while password reset remains allowed', async ({
  page,
}) => {
  const writes = await mockApi(page, 'super_admin', false, false, true)
  await login(page)
  await expect(page.getByRole('button', { name: '分配角色', exact: true })).toBeDisabled()
  await expect(page.getByRole('button', { name: '禁用', exact: true })).toBeDisabled()
  await expect(page.getByRole('button', { name: '编辑', exact: true })).toBeEnabled()
  await expect(page.getByRole('button', { name: '重置密码', exact: true })).toBeEnabled()
  expect(writes.some((write) => /\/(role|status)$/.test(write.path))).toBe(false)
})

test('admin cannot operate on a higher-role row even if it appears in a stale list', async ({
  page,
}) => {
  await mockApi(page, 'admin')
  await page.route('**/api/v1/admin/users?*', (route) =>
    route.fulfill({
      json: {
        data: { items: [{ ...baseUser, role: 'super_admin' }], total: 1, page: 1, page_size: 20 },
        request_id: 'scope-test',
      },
    }),
  )
  await login(page)
  await expect(page.getByRole('cell', { name: '超级管理员', exact: true })).toBeVisible()
  await expect(page.getByRole('button', { name: '分配角色', exact: true })).toBeDisabled()
  for (const name of ['禁用', '编辑', '重置密码']) {
    await expect(page.getByRole('button', { name, exact: true })).toBeDisabled()
  }
})

test('chosen self-reset returns to login without a temporary credential', async ({ page }) => {
  await mockApi(page, 'super_admin', false, false, true)
  await login(page)
  await page.getByRole('button', { name: '重置密码', exact: true }).click()
  await expect(page.getByRole('button', { name: '确认', exact: true })).toBeEnabled()
  await page.getByLabel('新密码', { exact: true }).fill('Abcd1234')
  await page.getByLabel('确认新密码', { exact: true }).fill('Abcd1234')
  await page.getByRole('button', { name: '确认', exact: true }).click()
  await expect(page).toHaveURL('/login')
  await expect(page.getByTestId('temporary-credential')).toHaveCount(0)
})

test('self-reset keeps the one-time password visible until acknowledged', async ({ page }) => {
  await mockApi(page, 'super_admin', false, false, true)
  let listRequests = 0
  page.on('request', (request) => {
    if (request.method() === 'GET' && new URL(request.url()).pathname.endsWith('/admin/users'))
      listRequests++
  })
  await login(page)
  await page.getByRole('button', { name: '重置密码', exact: true }).click()
  await expect(page.getByRole('button', { name: '确认', exact: true })).toBeEnabled()
  await page.getByRole('button', { name: '确认', exact: true }).click()
  await expect(page.getByTestId('temporary-credential')).toHaveText('SelfResetCredential123')
  expect(listRequests).toBe(1)
  await page.getByRole('button', { name: '已妥善记录，关闭' }).click()
  await expect(page).toHaveURL('/login')
  await expect(page.getByTestId('temporary-credential')).toHaveCount(0)
})

test('PrimeVue filters preserve server query values', async ({ page }) => {
  await mockApi(page)
  await login(page)
  await expect(page.getByRole('combobox', { name: '角色', exact: true })).toHaveText('全部角色')
  await expect(page.getByRole('combobox', { name: '状态', exact: true })).toHaveText('全部状态')
  await chooseOption(page, page.getByRole('combobox', { name: '角色', exact: true }), '工程师')
  await chooseOption(page, page.getByRole('combobox', { name: '状态', exact: true }), '已禁用')
  await chooseOption(page, page.getByRole('combobox', { name: '排序', exact: true }), '展示名')
  const request = page.waitForRequest((request) => {
    const url = new URL(request.url())
    return url.pathname.endsWith('/admin/users') && url.searchParams.get('role') === 'engineer'
  })
  await page.getByRole('button', { name: '查询', exact: true }).click()
  const url = new URL((await request).url())
  expect(Object.fromEntries(url.searchParams)).toMatchObject({
    role: 'engineer',
    status: 'disabled',
    sort: 'display_name',
    page: '1',
  })
})

test('dialog traps focus and restores it to the opening action on Escape', async ({ page }) => {
  await mockApi(page)
  await login(page)
  const opener = page.getByRole('button', { name: '创建用户' })
  await opener.click()
  const dialog = page.getByRole('dialog')
  await dialog.getByLabel('登录标识', { exact: true }).focus()
  await page.keyboard.press('Shift+Tab')
  await page.keyboard.press('Shift+Tab')
  expect(await dialog.evaluate((element) => element.contains(document.activeElement))).toBe(true)
  await page.keyboard.press('Escape')
  await expect(dialog).toHaveCount(0)
  await expect(opener).toBeFocused()
})

test('saving blocks dialog dismissal until the write finishes', async ({ page }) => {
  await mockApi(page)
  let release!: () => void
  const pending = new Promise<void>((resolve) => {
    release = resolve
  })
  await page.route('**/api/v1/admin/users', async (route) => {
    if (route.request().method() !== 'POST') return route.fallback()
    await pending
    return route.fulfill({
      status: 201,
      json: {
        data: {
          user: baseUser,
          temporary_credential: 'OnceOnlyCredential123',
          temporary_credential_expires_at: Date.now() + 86400000,
        },
        request_id: 'pending-write',
      },
    })
  })
  await login(page)
  await page.getByRole('button', { name: '创建用户' }).click()
  await page.getByLabel('登录标识', { exact: true }).fill('pending')
  await page.getByLabel('展示名', { exact: true }).fill('Pending')
  await page.getByRole('button', { name: '确认', exact: true }).click()
  const dialog = page.getByRole('dialog')
  try {
    await expect(dialog.getByRole('button', { name: '关闭', exact: true })).toBeDisabled()
    await page.keyboard.press('Escape')
    await expect(dialog).toBeVisible()
    await expect(dialog.getByRole('button', { name: '取消', exact: true })).toBeDisabled()
  } finally {
    release()
  }
  await expect(page.getByTestId('temporary-credential')).toHaveText('OnceOnlyCredential123')
  await page.getByRole('button', { name: '已妥善记录，关闭' }).click()
  await expect(dialog).toHaveCount(0)
})

test.describe('local date/time query contract', () => {
  test.use({ timezoneId: 'Asia/Shanghai' })
  test('picker input retains seconds and submits UTC timestamps', async ({ page }) => {
    await mockApi(page)
    await login(page)
    await page.getByRole('link', { name: '审计日志' }).click()
    await page.getByLabel('开始时间（本地）').fill('2026-10-13 08:15:06')
    await page.getByLabel('结束时间（本地）').fill('2026-10-13 09:16:07')
    const request = page.waitForRequest((request) => {
      const url = new URL(request.url())
      return url.pathname.endsWith('/admin/audit-logs') && url.searchParams.has('from')
    })
    await page.getByRole('button', { name: '查询日志' }).click()
    const params = new URL((await request).url()).searchParams
    expect(params.get('from')).toBe('2026-10-13T00:15:06Z')
    expect(params.get('to')).toBe('2026-10-13T01:16:07Z')
    await page.getByRole('tab', { name: '用户行为' }).click()
    await page.getByLabel('开始时间（本地）').fill('2026-10-13 08:15:06')
    await page.getByLabel('结束时间（本地）').fill('2026-10-13 09:16:07')
    const behavior = page.waitForRequest(
      (request) =>
        new URL(request.url()).pathname.endsWith('/admin/behavior-events') &&
        new URL(request.url()).searchParams.has('from'),
    )
    await page.getByRole('button', { name: '查询事件' }).click()
    expect(new URL((await behavior).url()).searchParams.get('from')).toBe('2026-10-13T00:15:06Z')
  })
  test('invalid manual input cannot submit a stale date', async ({ page }) => {
    await mockApi(page)
    await login(page)
    await page.getByRole('link', { name: '审计日志' }).click()
    const input = page.getByLabel('开始时间（本地）')
    await input.fill('2026-10-13 08:15:06')
    await input.fill('2026-02-30 08:15:06')
    expect(await input.evaluate((element: HTMLInputElement) => element.checkValidity())).toBe(false)
    await input.hover()
    expect(await input.evaluate((element) => getComputedStyle(element).boxShadow)).toContain(
      'rgb(239, 68, 68)',
    )
    await input.fill('2026-10-13 08:15:06')
    expect(await input.evaluate((element: HTMLInputElement) => element.checkValidity())).toBe(true)
    await page.getByRole('button', { name: '重置筛选' }).click()
    await expect(input).toHaveValue('')
  })
})

for (const theme of ['light', 'dark']) {
  test(`editable event dropdown keeps Lumina ${theme} surfaces and submits selected codes`, async ({
    page,
  }) => {
    await mockApi(page)
    await login(page)
    await page.getByRole('link', { name: '审计日志' }).click()
    await page.evaluate((theme) => {
      document.documentElement.dataset.theme = theme
    }, theme)
    const input = page.getByRole('combobox', { name: '事件名称', exact: true })
    await input.focus()
    const root = input.locator('..')
    const field = await input.evaluate((element) => {
      const style = getComputedStyle(element)
      const bounds = element.getBoundingClientRect()
      const container = element.parentElement!.getBoundingClientRect()
      return {
        border: style.borderTopWidth,
        shadow: style.boxShadow,
        background: style.backgroundColor,
        verticalOffset: Math.abs(
          bounds.top + bounds.height / 2 - container.top - container.height / 2,
        ),
      }
    })
    expect(field).toMatchObject({ border: '0px', shadow: 'none', background: 'rgba(0, 0, 0, 0)' })
    expect(field.verticalOffset).toBeLessThan(1)
    const readonly = page.getByRole('combobox', { name: '结果', exact: true })
    expect(await input.evaluate((element) => getComputedStyle(element).fontSize)).toBe(
      await readonly.evaluate((element) => getComputedStyle(element).fontSize),
    )
    expect(
      await root.evaluate((element) => Number.parseFloat(getComputedStyle(element).borderRadius)),
    ).toBeGreaterThan(0)
    await root.locator('.p-select-dropdown').click()
    const popup = page.locator('.p-select-overlay')
    await expect(popup).toBeVisible()
    await expect(popup.locator('.p-select-list-container')).toHaveClass(/lumina-scrollbar/)
    await expect(page.getByRole('option', { name: '登录成功', exact: true })).toContainText(
      'auth.login.success',
    )
    const surface = await popup.evaluate((element) => {
      const style = getComputedStyle(element)
      return {
        background: style.backgroundColor,
        radius: Number.parseFloat(style.borderRadius),
        border: style.borderTopWidth,
        shadow: style.boxShadow,
      }
    })
    const channels = surface.background.match(/[\d.]+/g)!.map(Number)
    expect(channels.length < 4 || channels[3]! >= 0.99).toBe(true)
    expect(surface.radius).toBeGreaterThan(0)
    expect(surface.border).toBe('1px')
    expect(surface.shadow).not.toBe('none')
    if (theme === 'dark') {
      expect(Math.max(...channels.slice(0, 3))).toBeLessThan(90)
      await page.screenshot({ path: '/tmp/szx-editable-audit-dark.png', fullPage: true })
    }
    await page.getByRole('option', { name: '登录成功', exact: true }).click()
    await expect(input).toHaveValue('登录成功')
    const selectedRequest = page.waitForRequest((request) =>
      new URL(request.url()).pathname.endsWith('/admin/audit-logs'),
    )
    await page.getByRole('button', { name: '查询日志' }).click()
    expect(new URL((await selectedRequest).url()).searchParams.get('event_search')).toBe(
      'auth.login.success',
    )
    await expect(page.getByRole('button', { name: '查询日志' })).toBeEnabled()
    await input.fill(' custom.event ')
    const customRequest = page.waitForRequest((request) =>
      new URL(request.url()).pathname.endsWith('/admin/audit-logs'),
    )
    await page.getByRole('button', { name: '查询日志' }).click()
    expect(new URL((await customRequest).url()).searchParams.get('event_search')).toBe(
      'custom.event',
    )
  })
}

test('behavior event dropdown offers returned codes and still accepts custom search', async ({
  page,
}) => {
  await mockApi(page)
  await login(page)
  await page.getByRole('link', { name: '审计日志' }).click()
  await page.getByRole('tab', { name: '用户行为' }).click()
  const input = page.getByRole('combobox', { name: '事件名称', exact: true })
  await input.locator('..').locator('.p-select-dropdown').click()
  await page.getByRole('option', { name: 'report.view', exact: true }).click()
  await expect(input).toHaveValue('report.view')
  await input.fill(' client.custom ')
  const requested = page.waitForRequest((request) =>
    new URL(request.url()).pathname.endsWith('/admin/behavior-events'),
  )
  await page.getByRole('button', { name: '查询事件' }).click()
  expect(new URL((await requested).url()).searchParams.get('event_search')).toBe('client.custom')
  await expect(page.getByRole('button', { name: '查询事件' })).toBeEnabled()
  await page.getByRole('button', { name: '重置筛选' }).click()
  await expect(input).toHaveValue('')
  await page.getByRole('tab', { name: '安全审计' }).click()
  await expect(page.getByRole('combobox', { name: '事件名称', exact: true })).toHaveValue('')
})
