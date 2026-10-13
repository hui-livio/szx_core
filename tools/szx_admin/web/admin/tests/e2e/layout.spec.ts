import { test, expect, type Page } from '@playwright/test'

// The app follows browser language; layout checks use the Chinese UI explicitly.
test.use({ locale: 'zh-CN' })

async function signInWithRows(page: Page) {
  const user = {
    id: 'layout-admin',
    login_identifier: 'admin',
    display_name: '系统管理员',
    role: 'super_admin',
    status: 'active',
    version: 1,
    authz_version: 1,
    force_password_change: false,
    password_change_recommended: false,
    created_at: Date.now(),
    updated_at: Date.now(),
    last_login_at: null,
  }
  await page.route('**/api/v1/**', async (route) => {
    const path = new URL(route.request().url()).pathname
    let data: unknown
    if (path.endsWith('/auth/login')) {
      data = {
        user,
        access_token: 'layout-access',
        csrf_token: 'layout-csrf',
        expires_at: Date.now() + 900_000,
        expires_in: 900,
        authz_version: 1,
        token_type: 'Bearer',
      }
    } else if (path.endsWith('/client/me/permissions')) {
      data = {
        user_id: user.id,
        role: user.role,
        capabilities: ['user.read', 'audit.read'],
        authz_version: 1,
        status: 'active',
        force_password_change: false,
      }
    } else {
      const items = Array.from({ length: 20 }, (_, index) => {
        if (path.endsWith('/admin/users')) {
          return {
            ...user,
            id: `layout-user-${index}`,
            login_identifier: `operator-${index}`,
            display_name: `操作员 ${index}`,
            role: 'operator',
          }
        }
        return {
          id: `layout-event-${index}`,
          event_name: path.endsWith('/behavior-events') ? 'report.view' : 'auth.login',
          result: 'success',
          actor_user_id: user.id,
          actor_login_identifier: user.login_identifier,
          user_id: user.id,
          login_identifier: user.login_identifier,
          target_user_id: user.id,
          request_id: `layout-request-${index}`,
          details: { source: 'web' },
          created_at: Date.now(),
        }
      })
      data = { items, total: 40, page: 1, page_size: 20 }
    }
    await route.fulfill({ json: { data, request_id: 'layout-request' } })
  })
  await page.goto('/login')
  await page.locator('input[name="identifier"]').fill('admin')
  await page.locator('input[name="password"]').fill('TestPassword123')
  await page.locator('.login-submit').click()
  await expect(page).toHaveURL('/users')
}

async function expectOnlyTableToScroll(page: Page) {
  await expect(page.locator('.page-heading .eyebrow')).toHaveCount(0)
  const topGap = await page.locator('.page-heading').evaluate((element) => {
    return (
      element.getBoundingClientRect().top -
      document.querySelector('.topbar')!.getBoundingClientRect().bottom
    )
  })
  expect(topGap).toBeLessThan(24)
  const table = page.locator('.table-scroll')
  await expect(table.locator('tbody tr')).toHaveCount(20)
  await page.evaluate(() => document.fonts.ready)
  const fixed = page.locator('.topbar, .page-heading, .table-heading, .pagination, .app-footer')
  const positions = await fixed.evaluateAll((elements) =>
    elements.map((element) => element.getBoundingClientRect().top),
  )
  const height = await table.evaluate((element) => element.clientHeight)
  expect(height).toBeGreaterThan(70)
  await table.hover()
  await page.mouse.wheel(0, 800)
  await expect.poll(() => table.evaluate((element) => element.scrollTop)).toBeGreaterThan(0)
  expect(
    await fixed.evaluateAll((elements) =>
      elements.map((element) => element.getBoundingClientRect().top),
    ),
  ).toEqual(positions)
  expect(
    await page.evaluate(() => ({
      pageScroll: document.scrollingElement?.scrollTop,
      mainScroll: document.querySelector('.main')?.scrollTop,
      workspaceScroll: document.querySelector('.workspace')?.scrollTop,
    })),
  ).toMatchObject({ pageScroll: 0, mainScroll: 0, workspaceScroll: 0 })
  const bottomGap = await page.locator('.table-panel').evaluate((element) => {
    const footer = document.querySelector('.app-footer')!
    return footer.getBoundingClientRect().top - element.getBoundingClientRect().bottom
  })
  expect(bottomGap).toBeLessThan(60)
  await expect(page.locator('.pagination')).toBeInViewport()
  await expect(page.locator('.app-footer')).toBeInViewport()
  const dimensions = await page.evaluate(() => ({
    width: document.documentElement.scrollWidth,
    height: document.documentElement.scrollHeight,
    viewportWidth: innerWidth,
    viewportHeight: innerHeight,
  }))
  expect(dimensions.width).toBeLessThanOrEqual(dimensions.viewportWidth)
  expect(dimensions.height).toBeLessThanOrEqual(dimensions.viewportHeight)
}

for (const viewport of [
  { width: 2232, height: 1281 },
  { width: 1440, height: 900 },
  { width: 1366, height: 768 },
  { width: 390, height: 844 },
]) {
  test(`data tables scroll inside a fixed workspace at ${viewport.width}×${viewport.height}`, async ({
    page,
  }) => {
    await page.setViewportSize(viewport)
    await signInWithRows(page)
    await expectOnlyTableToScroll(page)
    if (viewport.width === 2232) {
      await page.screenshot({ path: 'test-results/users-layout-desktop.png' })
    }
    await page.locator('.sidebar a[href="/logs"]').click()
    await expect(page).toHaveURL('/logs')
    await expect(page.locator('.logs-page')).toBeVisible()
    await expectOnlyTableToScroll(page)
    await page.getByRole('tab', { name: '用户行为' }).click()
    await expect(page.locator('.table-scroll tbody tr').first()).toContainText('report.view')
    await expectOnlyTableToScroll(page)
  })
}

test('language focus surrounds the complete component and effects start disabled', async ({
  page,
}) => {
  await page.goto('/login')
  const select = page.locator('.language-switcher select')
  await select.focus()
  expect(
    await select.evaluate((element) => {
      const control = getComputedStyle(element)
      const container = getComputedStyle(element.parentElement!)
      return {
        outline: control.outlineStyle,
        innerShadow: control.boxShadow,
        shadow: container.boxShadow,
      }
    }),
  ).toMatchObject({
    outline: 'none',
    innerShadow: 'none',
    shadow: expect.not.stringMatching(/^none$/),
  })
  await expect(select.locator('option')).toHaveCount(6)
  await expect(select.locator('option[value="en-US"]')).toHaveText('English')
  await expect(select.locator('option[value="en-GB"]')).toHaveCount(0)
  await select.selectOption('en-US')
  await expect(page.locator('.login-submit')).toContainText('Sign in')
  await expect(page.locator('html')).toHaveAttribute('data-fx', 'lite')
  expect(await page.evaluate(() => document.getAnimations().length)).toBe(0)
})

for (const savedLocale of ['en-US', 'en-GB']) {
  test(`saved ${savedLocale} preference uses the single English option`, async ({ page }) => {
    await page.addInitScript((locale) => {
      localStorage.setItem('szx-admin-locale', locale)
    }, savedLocale)
    await page.goto('/login')
    await expect(page.locator('.language-switcher select')).toHaveValue('en-US')
    await expect(page.locator('.login-submit')).toContainText('Sign in')
    await expect(page.locator('html')).toHaveAttribute('lang', 'en-US')
  })
}

test.describe('English browser language', () => {
  test.use({ locale: 'en-GB' })

  test('UK browser language resolves to the single English option', async ({ page }) => {
    await page.goto('/login')
    await expect(page.locator('.language-switcher select')).toHaveValue('en-US')
    await expect(page.locator('.login-submit')).toContainText('Sign in')
    await expect(page.locator('html')).toHaveAttribute('lang', 'en-US')

    await page.locator('.language-switcher select').selectOption('zh-CN')
    await page.locator('.language-switcher select').selectOption('en-US')
    expect(await page.evaluate(() => localStorage.getItem('szx-admin-locale'))).toBe('en-US')
    await page.reload()
    await expect(page.locator('.language-switcher select')).toHaveValue('en-US')
    await expect(page.locator('.login-submit')).toContainText('Sign in')
  })
})
