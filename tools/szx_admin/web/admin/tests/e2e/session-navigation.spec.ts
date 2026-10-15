import { test, expect, type Page } from '@playwright/test'

test.use({ locale: 'zh-CN' })

function gate() {
  let release!: () => void
  const promise = new Promise<void>((resolve) => {
    release = resolve
  })
  return { promise, release }
}

async function mockSession(page: Page, role = 'super_admin', forcePassword = false) {
  const user = {
    id: 'navigation-user',
    login_identifier: 'admin',
    display_name: '测试账号',
    role,
    status: 'active',
    version: 1,
    authz_version: 1,
    force_password_change: forcePassword,
    password_change_recommended: false,
    created_at: Date.now(),
    updated_at: Date.now(),
    last_login_at: null,
  }
  await page.route('**/api/v1/**', (route) => {
    const path = new URL(route.request().url()).pathname
    if (path.endsWith('/auth/logout')) return route.fulfill({ status: 204 })
    const data = path.endsWith('/auth/login')
      ? {
          user,
          access_token: 'navigation-access',
          csrf_token: 'navigation-csrf',
          expires_at: Date.now() + 900_000,
          expires_in: 900,
          authz_version: 1,
          token_type: 'Bearer',
        }
      : path.endsWith('/client/me/permissions')
        ? {
            user_id: user.id,
            role,
            capabilities:
              role === 'admin' || role === 'super_admin'
                ? ['business.read', 'user.read', 'audit.read']
                : ['business.read'],
            authz_version: 1,
            status: 'active',
            force_password_change: forcePassword,
          }
        : { items: [], total: 0, page: 1, page_size: 20 }
    return route.fulfill({ json: { data, request_id: 'navigation-request' } })
  })
}

async function submitLogin(page: Page) {
  await page.goto('/login')
  await page.locator('input[name="identifier"]').fill('admin')
  await page.locator('input[name="password"]').fill('TestPassword123')
  await page.locator('.login-submit').click()
}

test('successful login removes the login UI while the destination module is still loading', async ({
  page,
}) => {
  await mockSession(page)
  const module = gate()
  const requested = gate()
  await page.route('**/src/pages/HomePage.vue*', async (route) => {
    requested.release()
    await module.promise
    await route.continue()
  })
  try {
    await submitLogin(page)
    await requested.promise
    await expect(page.locator('.app-shell')).toBeVisible()
    await expect(page.locator('.login-page')).toHaveCount(0)
    await expect(page.locator('.main [role="status"]')).toHaveText('加载中…')
    module.release()
    await expect(page).toHaveURL('/home')
    await expect(page.locator('.page-heading h1')).toHaveText('你好，测试账号')
  } finally {
    module.release()
  }
})

for (const role of ['super_admin', 'operator']) {
  test(`${role} sees the destination before its data response arrives`, async ({ page }) => {
    await mockSession(page, role)
    const response = gate()
    const requested = gate()
    const dataPath = '/client/me/permissions'
    await page.route(`**/api/v1${dataPath}*`, async (route) => {
      requested.release()
      await response.promise
      await route.fallback()
    })
    try {
      await submitLogin(page)
      await requested.promise
      await expect(page).toHaveURL('/home')
      await expect(page.locator('.login-page')).toHaveCount(0)
      await expect(page.locator('.page-heading .eyebrow')).toHaveCount(0)
      await expect(page.locator('.page-heading h1')).toHaveText('你好，测试账号')
      await expect(page.getByRole('button', { name: '加载中…' })).toBeDisabled()
      response.release()
      await expect(page.getByRole('button', { name: '刷新权限' })).toBeEnabled()
      await page.locator('.topbar').getByRole('button', { name: '退出', exact: true }).click()
      await expect(page).toHaveURL('/login')
      await expect(page.locator('.login-page')).toBeVisible()
      await expect(page.locator('.app-shell')).toHaveCount(0)
    } finally {
      response.release()
    }
  })
}

test('failed data loading reports an error in the destination rather than returning to login', async ({
  page,
}) => {
  await mockSession(page)
  await page.route('**/api/v1/client/me/permissions*', (route) =>
    route.fulfill({ status: 503, json: { code: 2216, request_id: 'navigation-error' } }),
  )
  await submitLogin(page)
  await expect(page).toHaveURL('/home')
  await expect(page.locator('.main [role="alert"]')).toContainText('服务暂不可用')
  await expect(page.locator('.login-page')).toHaveCount(0)
})

test('first login still enters the mandatory password page', async ({ page }) => {
  await mockSession(page, 'super_admin', true)
  await submitLogin(page)
  await expect(page).toHaveURL('/change-password')
  await expect(page.locator('.page-heading h1')).toHaveText('修改密码')
  await expect(page.locator('.page-heading .eyebrow')).toHaveCount(0)
  await expect(page.locator('.login-page')).toHaveCount(0)
  await expect(page.getByRole('button', { name: '跳过，稍后修改' })).toHaveCount(0)
})

for (const role of ['operator', 'engineer', 'admin', 'super_admin']) {
  test(`${role} defaults to home after normal login`, async ({ page }) => {
    await mockSession(page, role)
    await submitLogin(page)
    await expect(page).toHaveURL('/home')
    await expect(page.locator('.page-heading h1')).toHaveText('你好，测试账号')
    await expect(page.getByRole('link', { name: '工作台', exact: true })).toHaveClass(
      /router-link-active/,
    )
    const protectedLinks = page.locator('.sidebar a[href="/users"]')
    await expect(protectedLinks).toHaveCount(role === 'admin' || role === 'super_admin' ? 1 : 0)
  })
}
