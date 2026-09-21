import { test, expect, type Page } from '@playwright/test'
import { createHash } from 'node:crypto'
import type { Role, User } from '../../src/api/types'

const baseUser: User = {
  id: '10000000-0000-4000-8000-000000000002',
  username: 'alice',
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
    username: 'admin',
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
            actor_username: actor.username,
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
            username: actor.username,
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
  await page.getByLabel('登录账号').fill('admin')
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
    identifier: 'admin',
    password_hash: expect.stringMatching(/^[a-f0-9]{64}$/),
  })
  await page.getByRole('button', { name: '创建用户' }).click()
  await page.getByLabel('用户名', { exact: true }).fill('bob')
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
  await page.getByLabel('用户名', { exact: true }).fill('chosen')
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
    username: 'chosen',
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
    await page.getByLabel('用户名', { exact: true }).fill('simple')
    await page.getByLabel('展示名', { exact: true }).fill('Simple')
    const select = page.getByRole('dialog').getByLabel('角色', { exact: true })
    await select.selectOption('admin')
    await expect(page.locator('#initial-password-rule')).toContainText('至少 8 个字符')
    await page.getByLabel('初始密码', { exact: true }).fill('123456')
    await page.getByLabel('确认初始密码', { exact: true }).fill('123456')
    await page.getByRole('button', { name: '确认', exact: true }).click()
    await expect(page.getByRole('alert')).toContainText('至少 8 个字符')
    expect(writes.filter((write) => write.path === '/admin/users')).toHaveLength(0)
    await select.selectOption(role)
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
  await expect(page.getByRole('button', { name: '分配角色' })).toHaveCount(0)
  await page.getByRole('button', { name: '创建用户' }).click()
  await expect(
    page.getByRole('dialog').getByRole('combobox', { name: '角色', exact: true }).locator('option'),
  ).toHaveText(['操作员', '工程师'])
  await page.keyboard.press('Escape')
  await page.getByRole('link', { name: '审计日志' }).click()
  await expect(page.getByRole('columnheader', { name: '用户名', exact: true })).toBeVisible()
  await expect(page.getByRole('cell', { name: 'admin', exact: true })).toBeVisible()
  await expect(page.getByRole('searchbox', { name: '搜索用户', exact: true })).toHaveAttribute(
    'placeholder',
    '搜索展示名、用户名或 UID',
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
  await expect(input).toHaveAttribute('list', 'audit-event-options')
  await expect(page.locator('#audit-event-options option[value="登录成功"]')).toHaveAttribute(
    'label',
    'auth.login.success',
  )
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
  await page.getByLabel('开始时间（本地）').fill('2026-09-01T00:00')
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
  await expect(page.getByRole('cell', { name: /report\.view/ }).getByText('report.view')).toBeVisible()
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
  await expect(page.getByRole('button', { name: '分配角色', exact: true })).toHaveCount(0)
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
