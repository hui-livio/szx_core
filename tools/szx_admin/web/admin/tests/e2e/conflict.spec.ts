import { test, expect } from '@playwright/test'

test('version conflict blocks resubmission until the user reloads', async ({ page }) => {
  let version = 1
  let attempts = 0
  const user = {
    id: '10000000-0000-4000-8000-000000000001',
    login_identifier: 'admin',
    display_name: '管理员',
    role: 'super_admin',
    status: 'active',
    version,
    authz_version: 1,
    force_password_change: false,
    password_change_recommended: false,
    last_login_at: null,
    created_at: 0,
    updated_at: 0,
  }
  await page.route('**/api/v1/**', async (route) => {
    const request = route.request()
    const path = new URL(request.url()).pathname
    const reply = (data: unknown) => route.fulfill({ json: { data, request_id: 'conflict-test' } })
    if (path.endsWith('/auth/login'))
      return reply({
        user,
        access_token: 'access',
        csrf_token: 'csrf',
        expires_at: Date.now() + 900000,
      })
    if (path.endsWith('/admin/users'))
      return reply({ items: [user], total: 1, page: 1, page_size: 20 })
    if (request.method() === 'PATCH') {
      attempts++
      if (attempts === 1) {
        version = 2
        return route.fulfill({ status: 409, json: { code: 2209, request_id: 'conflict-test' } })
      }
      expect(request.headers()['if-match']).toBe('2')
      return reply({ ...user, version: 3 })
    }
    return reply({ ...user, version })
  })
  await page.goto('/login')
  await page.locator('input[name="identifier"]').fill('admin')
  await page.getByLabel('密码', { exact: true }).fill('test')
  await page.getByRole('button', { name: '登录控制台' }).click()
  await page.getByRole('button', { name: '编辑', exact: true }).click()
  await expect(page.getByRole('button', { name: '确认', exact: true })).toBeEnabled()
  await page.getByLabel('展示名', { exact: true }).fill('Updated')
  await page.getByRole('button', { name: '确认', exact: true }).click()
  await expect(page.getByRole('alert')).toContainText('2209')
  await expect(page.getByRole('button', { name: '确认', exact: true })).toBeDisabled()
  await page.getByRole('button', { name: '重新加载用户' }).click()
  await expect(page.getByRole('button', { name: '确认', exact: true })).toBeEnabled()
  await page.getByLabel('展示名', { exact: true }).fill('Updated again')
  await page.getByRole('button', { name: '确认', exact: true }).click()
  await expect(page.getByRole('dialog')).toHaveCount(0)
  expect(attempts).toBe(2)
})
