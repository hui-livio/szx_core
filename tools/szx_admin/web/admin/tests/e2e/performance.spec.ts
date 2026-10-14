import { test, expect, type Page } from '@playwright/test'

const metric = (usage = 40) => ({
  status: 'ok',
  error_code: null,
  usage_percent: usage,
  total_bytes: 16 * 1024 ** 3,
  used_bytes: 6 * 1024 ** 3,
  available_bytes: 10 * 1024 ** 3,
  free_bytes: null,
})
async function setup(page: Page, role = 'admin', permission = true) {
  let loggedIn = false
  let count = 0
  let instance = 'instance-a'
  let memoryUnavailable = false
  let fail = false
  const user = {
    id: 'admin',
    login_identifier: 'admin',
    display_name: 'Admin',
    role,
    status: 'active',
    force_password_change: false,
    password_change_recommended: false,
    version: 1,
    authz_version: 1,
    created_at: 0,
    updated_at: 0,
    last_login_at: null,
  }
  await page.route('**/api/v1/**', async (route) => {
    const path = new URL(route.request().url()).pathname
    const reply = (data: unknown) =>
      route.fulfill({ json: { data, request_id: 'performance-test' } })
    if (path.endsWith('/auth/refresh') || path.endsWith('/auth/login')) {
      if (path.endsWith('/auth/login')) loggedIn = true
      if (!loggedIn) return route.fulfill({ status: 401, json: { code: 2200 } })
      return reply({
        user,
        access_token: 'token',
        csrf_token: 'csrf',
        token_type: 'Bearer',
        expires_at: Date.now() + 900_000,
        expires_in: 900,
        authz_version: 1,
      })
    }
    if (path.endsWith('/client/me/permissions'))
      return reply({
        user_id: user.id,
        role,
        capabilities: permission ? ['system.metrics.read'] : [],
        authz_version: 1,
      })
    if (path.endsWith('/metrics/history'))
      return reply({
        instance_id: instance,
        sample_interval_ms: 2000,
        window_seconds: 900,
        samples: [{ sequence: count, sampled_at: Date.now(), cpu: metric(25), memory: metric(40) }],
      })
    if (path.endsWith('/metrics')) {
      count++
      if (fail) return route.fulfill({ status: 503, json: { code: 2216, request_id: 'offline' } })
      return reply({
        instance_id: instance,
        sequence: count,
        sampled_at: Date.now(),
        cpu: metric(25),
        memory: memoryUnavailable
          ? { ...metric(), status: 'unavailable', error_code: 2216, usage_percent: null }
          : metric(40),
        platform: 'linux',
        logical_cpus: 8,
        sample_interval_ms: 2000,
        disk_interval_ms: 30000,
        disks: ['/', '/data'].map((path) => ({
          id: path,
          path,
          sampled_at: Date.now(),
          capacity: { ...metric(60), free_bytes: 10 * 1024 ** 3 },
        })),
      })
    }
    if (path.endsWith('/auth/logout')) return reply(null)
    return reply({ items: [], total: 0, page: 1, page_size: 20 })
  })
  await page.goto('/login')
  await page.locator('input[name="identifier"]').fill('admin')
  await page.locator('input[name="password"]').fill('TestPassword123')
  await page.locator('.login-submit').click()
  await expect(page).not.toHaveURL('/login')
  return {
    count: () => count,
    restart: () => {
      instance = 'instance-b'
      count = 0
    },
    unavailable: () => {
      memoryUnavailable = true
    },
    fail: () => {
      fail = true
    },
  }
}

test('one card per metric/volume, ECharts SVG and pause/resume/leave cleanup', async ({ page }) => {
  const state = await setup(page)
  await page.getByRole('link', { name: '机器性能' }).click()
  await expect(page.locator('.performance-grid .lumina-card')).toHaveCount(4)
  await expect(page.getByRole('img', { name: 'CPU 使用率趋势' }).locator('svg')).toHaveCount(1)
  await expect(page.getByRole('heading', { name: 'CPU', exact: true })).toBeVisible()
  await expect(page.locator('.performance-value').first()).toHaveText('25%')
  await page.getByRole('button', { name: '暂停', exact: true }).click()
  const pausedCount = state.count()
  await page.waitForTimeout(2300)
  expect(state.count()).toBe(pausedCount)
  await expect(page.getByText('已暂停自动刷新')).toBeVisible()
  await page.getByRole('button', { name: '继续', exact: true }).click()
  await expect.poll(state.count).toBeGreaterThan(pausedCount)
  await page.getByRole('link', { name: '工作台', exact: true }).click()
  const leftCount = state.count()
  await page.waitForTimeout(2300)
  expect(state.count()).toBe(leftCount)
})

test('partial unavailable, server restart, request failure retain explicit state', async ({
  page,
}) => {
  const state = await setup(page)
  await page.getByRole('link', { name: '机器性能' }).click()
  await expect(page.locator('.performance-value').first()).toHaveText('25%')
  state.unavailable()
  await expect(page.getByText('暂时无法采集', { exact: true })).toBeVisible()
  state.restart()
  await expect.poll(state.count).toBeGreaterThan(0)
  await expect(page.getByRole('img', { name: 'CPU 使用率趋势' })).toBeVisible()
  state.fail()
  await expect(page.getByRole('alert')).toContainText('2216')
  await expect(page.locator('.performance-grid .lumina-card')).toHaveCount(4)
})

test('hidden document stops polling; showing document resumes', async ({ page }) => {
  const state = await setup(page)
  await page.getByRole('link', { name: '机器性能' }).click()
  await expect(page.locator('.performance-value').first()).toHaveText('25%')
  await page.evaluate(() => {
    Object.defineProperty(document, 'hidden', { configurable: true, get: () => true })
    document.dispatchEvent(new Event('visibilitychange'))
  })
  const count = state.count()
  await page.waitForTimeout(2300)
  expect(state.count()).toBe(count)
  await page.evaluate(() => {
    Object.defineProperty(document, 'hidden', { configurable: true, get: () => false })
    document.dispatchEvent(new Event('visibilitychange'))
  })
  await expect.poll(state.count).toBeGreaterThan(count)
})

test('mobile cards stay inside viewport; dark/full/lite/reduced-motion charts render', async ({
  page,
}) => {
  const errors: string[] = []
  page.on('pageerror', (error) => errors.push(error.message))
  await page.setViewportSize({ width: 390, height: 844 })
  await setup(page)
  await page.getByRole('link', { name: '机器性能' }).click()
  await expect(page.locator('.lumina-chart svg')).toHaveCount(4)
  for (const fx of ['lite', 'full']) {
    await page.evaluate((fx) => {
      document.documentElement.dataset.theme = 'dark'
      document.documentElement.dataset.fx = fx
    }, fx)
    await page.emulateMedia({ reducedMotion: 'reduce' })
    expect(
      await page
        .locator('.lumina-card')
        .first()
        .evaluate((element) => getComputedStyle(element).backgroundColor),
    ).not.toBe('rgba(0, 0, 0, 0)')
    await expect(page.locator('.lumina-chart svg')).toHaveCount(4)
  }
  expect(await page.evaluate(() => document.documentElement.scrollWidth)).toBeLessThanOrEqual(390)
  expect(errors).toEqual([])
})

for (const role of ['operator', 'engineer', 'admin']) {
  test(`metrics route and navigation denied without capability (${role})`, async ({ page }) => {
    const state = await setup(page, role, false)
    await expect(page.getByRole('link', { name: '机器性能' })).toHaveCount(0)
    await page.evaluate(() => {
      history.pushState({}, '', '/system/performance')
      window.dispatchEvent(new PopStateEvent('popstate'))
    })
    await expect(page).toHaveURL('/home')
    expect(state.count()).toBe(0)
  })
}
