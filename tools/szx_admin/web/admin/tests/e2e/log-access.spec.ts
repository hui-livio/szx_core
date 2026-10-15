import { expect, test, type Page } from '@playwright/test'

async function setup(page: Page, initial: string[]) {
  let capabilities = initial
  let roleVersion = 1
  let expireLog = false
  const reads = { audit: 0, behavior: 0 }
  const user = {
    id: 'log-reader',
    login_identifier: 'reader',
    display_name: 'Reader',
    role: 'custom_log_reader',
    status: 'active',
    force_password_change: false,
    password_change_recommended: false,
    version: 1,
    authz_version: 1,
    role_id: 'custom-log-role',
    created_at: 0,
    updated_at: 0,
    last_login_at: null,
  }
  await page.route('**/api/v1/**', async (route) => {
    const path = new URL(route.request().url()).pathname.replace('/api/v1', '')
    const reply = (data: unknown) => route.fulfill({ json: { data, request_id: 'log-access' } })
    if (path === '/auth/login' || path === '/auth/refresh')
      return reply({
        user: { ...user, role_authz_version: roleVersion },
        access_token: `access-${roleVersion}`,
        token_type: 'Bearer',
        csrf_token: 'csrf',
        expires_at: Date.now() + 900000,
        expires_in: 900,
        authz_version: 1,
      })
    if (path === '/client/me/permissions')
      return reply({
        user_id: user.id,
        role: user.role,
        authz_version: 1,
        capabilities,
        manageable_roles: [],
      })
    const audit = path === '/admin/audit-logs'
    const behavior = path === '/admin/behavior-events'
    if (audit || behavior) {
      reads[audit ? 'audit' : 'behavior']++
      if (expireLog) {
        expireLog = false
        return route.fulfill({ status: 401, json: { code: 2200 } })
      }
      if (!capabilities.includes(audit ? 'audit.read' : 'behavior.read'))
        return route.fulfill({ status: 403, json: { code: 2204 } })
      return reply({
        items: [
          {
            id: audit ? 'audit-1' : 'behavior-1',
            event_name: audit ? 'auth.login' : 'report.view',
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
        ],
        total: 1,
        page: 1,
        page_size: 20,
      })
    }
    return reply({ items: [] })
  })
  await page.goto('/login')
  await page.locator('input[name="identifier"]').fill('reader')
  await page.locator('input[name="password"]').fill('TestPassword123')
  await page.locator('.login-submit').click()
  await expect(page).toHaveURL('/home')
  return {
    reads,
    setCapabilities: (next: string[]) => {
      capabilities = next
      roleVersion++
      expireLog = true
    },
  }
}

for (const capabilities of [['audit.read'], ['behavior.read'], ['audit.read', 'behavior.read']]) {
  test(`log access follows independent capabilities: ${capabilities.join(',')}`, async ({
    page,
  }) => {
    const state = await setup(page, capabilities)
    await expect(page.locator('.home-navigation a[href="/logs"]')).toBeVisible()
    await page.locator('.sidebar nav a[href="/logs"]').click()
    await expect(page).toHaveURL('/logs')
    const audit = capabilities.includes('audit.read')
    await expect(page.getByRole('tab', { name: '安全审计', exact: true })).toHaveCount(
      audit ? 1 : 0,
    )
    await expect(page.getByRole('tab', { name: '用户行为', exact: true })).toHaveCount(
      capabilities.includes('behavior.read') ? 1 : 0,
    )
    await expect(
      page.getByRole('tab', { name: audit ? '安全审计' : '用户行为', exact: true }),
    ).toHaveAttribute('aria-selected', 'true')
    await expect(page.locator('.table-scroll tbody tr')).toHaveCount(1)
    if (!audit) expect(state.reads.audit).toBe(0)
    if (!capabilities.includes('behavior.read')) expect(state.reads.behavior).toBe(0)
    await page.getByRole('button', { name: audit ? '刷新日志' : '刷新事件', exact: true }).click()
    await expect(page.locator('.table-scroll tbody tr')).toHaveCount(1)
    await expect(page.getByRole('alert')).toHaveCount(0)
    await page.locator('.sidebar nav a[href="/home"]').click()
    await page.evaluate(() => {
      history.pushState({}, '', '/logs')
      window.dispatchEvent(new PopStateEvent('popstate'))
    })
    await expect(page).toHaveURL('/logs')
    await expect(page.locator('.table-scroll tbody tr')).toHaveCount(1)
  })
}

test('without either log capability both navigation and direct access fail closed', async ({
  page,
}) => {
  const state = await setup(page, ['business.read'])
  await expect(page.locator('a[href="/logs"]')).toHaveCount(0)
  await page.evaluate(() => {
    history.pushState({}, '', '/logs')
    window.dispatchEvent(new PopStateEvent('popstate'))
  })
  await expect(page).toHaveURL('/home')
  expect(state.reads).toEqual({ audit: 0, behavior: 0 })
})

test('revoking the active audit tab automatically opens the remaining behavior tab', async ({
  page,
}) => {
  const state = await setup(page, ['audit.read', 'behavior.read'])
  await page.locator('.sidebar nav a[href="/logs"]').click()
  await expect(page.getByRole('tab', { name: '安全审计', exact: true })).toHaveAttribute(
    'aria-selected',
    'true',
  )
  await expect(page.locator('.table-scroll tbody tr')).toHaveCount(1)
  state.setCapabilities(['behavior.read'])
  await page.getByRole('button', { name: '刷新日志', exact: true }).click()
  await expect(page.getByRole('tab', { name: '安全审计', exact: true })).toHaveCount(0)
  await expect(page.getByRole('tab', { name: '用户行为', exact: true })).toHaveAttribute(
    'aria-selected',
    'true',
  )
  await expect(page.locator('.table-scroll')).toContainText('report.view')
  await expect(page.getByRole('alert')).toHaveCount(0)
  await expect(page).toHaveURL('/logs')
})

test('revocation switches to an allowed tab and discards late privileged results', async ({
  page,
}) => {
  const state = await setup(page, ['audit.read', 'behavior.read'])
  await page.locator('.sidebar nav a[href="/logs"]').click()
  await expect(page.locator('.table-scroll tbody tr')).toHaveCount(1)
  let release!: () => void
  let started!: () => void
  const blocked = new Promise<void>((resolve) => {
    release = resolve
  })
  const arrived = new Promise<void>((resolve) => {
    started = resolve
  })
  await page.route('**/api/v1/admin/audit-logs?**', async (route) => {
    started()
    await blocked
    await route
      .fulfill({
        json: {
          data: {
            items: [
              {
                id: 'stale',
                event_name: 'auth.login',
                actor_login_identifier: 'stale-privileged-audit',
                result: 'success',
                details: {},
                created_at: 0,
              },
            ],
            total: 1,
            page: 1,
            page_size: 20,
          },
          request_id: 'stale',
        },
      })
      .catch(() => {})
  })
  try {
    await page.getByRole('button', { name: '刷新日志', exact: true }).click()
    await arrived
    await page.getByRole('tab', { name: '用户行为', exact: true }).click()
    state.setCapabilities(['behavior.read'])
    await page.getByRole('button', { name: '刷新事件', exact: true }).click()
    await expect(page.getByRole('tab', { name: '安全审计', exact: true })).toHaveCount(0)
    await expect(page.getByRole('tab', { name: '用户行为', exact: true })).toHaveAttribute(
      'aria-selected',
      'true',
    )
    await expect(page.locator('.table-scroll')).toContainText('report.view')
    release()
    await expect(page.getByText('stale-privileged-audit', { exact: true })).toHaveCount(0)
    await expect(page.getByRole('alert')).toHaveCount(0)
    state.setCapabilities([])
    await page.getByRole('button', { name: '刷新事件', exact: true }).click()
    await expect(page).toHaveURL('/home')
    await expect(page.locator('a[href="/logs"]')).toHaveCount(0)
  } finally {
    release()
  }
})
