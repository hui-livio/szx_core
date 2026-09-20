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
        temporary_credential: 'SelfResetCredential123',
        temporary_credential_expires_at: Date.now() + 86400000,
      })
    if (path.startsWith('/admin/users/')) return reply({ ...baseUser, ...body, version: 4 })
    if (path === '/admin/user-logs')
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
  await password.fill('short1')
  await confirmation.fill('short1')
  await page.getByRole('button', { name: '确认', exact: true }).click()
  await expect(page.getByRole('alert')).toContainText('至少 12 个字符')
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

test('forced password flow cannot be skipped and hashes both passwords', async ({ page }) => {
  const writes = await mockApi(page, 'super_admin', true)
  await login(page)
  await expect(page).toHaveURL('/change-password')
  await expect(page.getByRole('button', { name: /跳过/ })).toHaveCount(0)
  await page.getByLabel('当前密码', { exact: true }).fill('TestPassword123')
  await page.getByLabel('新密码', { exact: true }).fill('ChangedPassword456')
  await page.getByLabel('确认新密码', { exact: true }).fill('ChangedPassword456')
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
test('login remains usable at mobile width', async ({ page }) => {
  await page.setViewportSize({ width: 390, height: 844 })
  await page.goto('/login')
  await expect(page.getByRole('button', { name: '登录控制台' })).toBeVisible()
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true)
  await page.screenshot({ path: 'test-results/login-mobile.png', fullPage: true })
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
