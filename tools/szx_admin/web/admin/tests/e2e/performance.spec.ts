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
async function setup(page: Page, role = 'admin', permission = true, diskPaths = ['/', '/data']) {
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
        disks: diskPaths.map((path) => ({
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
    setVolumes: (paths: string[]) => {
      diskPaths = paths
    },
  }
}

test('one card per metric/volume, ECharts SVG and pause/resume/leave cleanup', async ({ page }) => {
  const state = await setup(page)
  await page.locator('.sidebar nav').getByRole('link', { name: '机器性能' }).click()
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
  await page.locator('.sidebar nav').getByRole('link', { name: '机器性能' }).click()
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
  await page.locator('.sidebar nav').getByRole('link', { name: '机器性能' }).click()
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
  await page.locator('.sidebar nav').getByRole('link', { name: '机器性能' }).click()
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

const sevenCardVolumes = [
  '/usr/lib/modules/6.18.33.2-microsoft-standard-WSL2',
  '/',
  '/usr/lib/wsl/lib',
  '/mnt/wslg/version.txt',
  '/mnt/wslg/doc',
]

async function readCardRows(page: Page) {
  return page.locator('.performance-grid').evaluate((grid) => {
    const bounds = grid.getBoundingClientRect()
    const gap = Number.parseFloat(getComputedStyle(grid).columnGap)
    const rows: { x: number; y: number; width: number }[][] = []
    for (const card of grid.querySelectorAll('.lumina-card')) {
      const { x, y, width } = card.getBoundingClientRect()
      const row = rows.at(-1)
      if (row && Math.abs(row[0]!.y - y) < 1) row.push({ x, y, width })
      else rows.push([{ x, y, width }])
    }
    return { x: bounds.x, width: bounds.width, gap, rows }
  })
}

for (const total of [2, 3, 4, 5, 6, 7, 8, 9, 14, 34]) {
  test(`layout adapts to ${total} API-driven cards and fills every row`, async ({ page }) => {
    await page.setViewportSize({ width: 1920, height: 1000 })
    await setup(
      page,
      'admin',
      true,
      Array.from({ length: total - 2 }, (_, index) => `/volume-${index}`),
    )
    await page.locator('.sidebar nav').getByRole('link', { name: '机器性能' }).click()
    await expect(page.locator('.performance-grid .lumina-card')).toHaveCount(total)
    const layout = await readCardRows(page)
    const capacity = Math.max(1, Math.floor((layout.width + layout.gap) / (320 + layout.gap)))
    expect(layout.rows).toHaveLength(Math.ceil(total / capacity))
    const counts = layout.rows.map((row) => row.length)
    expect(Math.max(...counts) - Math.min(...counts)).toBeLessThanOrEqual(1)
    for (const row of layout.rows) {
      expect(row[0]!.x).toBeCloseTo(layout.x, 0)
      expect(row.at(-1)!.x + row.at(-1)!.width).toBeCloseTo(layout.x + layout.width, 0)
      for (const card of row) expect(card.width).toBeGreaterThanOrEqual(319)
    }
    if (total === 7) {
      expect(counts).toEqual([4, 3])
      await page.screenshot({ path: '/tmp/szx-performance-adaptive.png', fullPage: true })
    }
  })
}

test('live disk count and container resizing rebalance without losing chart instances', async ({
  page,
}) => {
  const errors: string[] = []
  page.on('pageerror', (error) => errors.push(error.message))
  await page.setViewportSize({ width: 1920, height: 1000 })
  const state = await setup(page, 'admin', true, sevenCardVolumes)
  await page.locator('.sidebar nav').getByRole('link', { name: '机器性能' }).click()
  await expect(page.locator('.performance-grid .lumina-card')).toHaveCount(7)
  const chart = page.getByRole('img', { name: 'CPU 使用率趋势' })
  const instance = await chart.getAttribute('_echarts_instance_')
  expect(instance).toBeTruthy()
  for (const diskCount of [1, 6, 0, 5]) {
    state.setVolumes(Array.from({ length: diskCount }, (_, index) => `/volume-${index}`))
    await expect(page.locator('.performance-grid .lumina-card')).toHaveCount(diskCount + 2)
    const layout = await readCardRows(page)
    for (const row of layout.rows)
      expect(row.at(-1)!.x + row.at(-1)!.width).toBeCloseTo(layout.x + layout.width, 0)
    expect(await chart.getAttribute('_echarts_instance_')).toBe(instance)
  }
  await page.setViewportSize({ width: 1024, height: 900 })
  await expect.poll(async () => (await readCardRows(page)).rows.length).toBeGreaterThan(2)
  await page.setViewportSize({ width: 390, height: 844 })
  await expect.poll(async () => (await readCardRows(page)).rows.length).toBe(7)
  await page.waitForTimeout(150)
  expect(errors).toEqual([])
})

test('seven cards become single-column without horizontal overflow on mobile', async ({ page }) => {
  await page.setViewportSize({ width: 390, height: 844 })
  await setup(page, 'admin', true, sevenCardVolumes)
  await page.locator('.sidebar nav').getByRole('link', { name: '机器性能' }).click()
  await expect(page.locator('.performance-grid .lumina-card')).toHaveCount(7)
  const bounds = await page.locator('.performance-grid .lumina-card').evaluateAll((cards) =>
    cards.map((card) => {
      const { x, y, width } = card.getBoundingClientRect()
      return { x, y, width }
    }),
  )
  for (let index = 1; index < bounds.length; index++) {
    expect(bounds[index]!.x).toBeCloseTo(bounds[0]!.x, 0)
    expect(bounds[index]!.width).toBeCloseTo(bounds[0]!.width, 0)
    expect(bounds[index]!.y).toBeGreaterThan(bounds[index - 1]!.y)
  }
  expect(await page.evaluate(() => document.documentElement.scrollWidth)).toBeLessThanOrEqual(390)
})

test('main scrollbar is rounded, transparent, theme-aware and keeps native scrolling', async ({
  page,
}) => {
  await setup(page, 'admin', true, sevenCardVolumes)
  await page.locator('.sidebar nav').getByRole('link', { name: '机器性能' }).click()
  await expect(page.locator('.performance-grid .lumina-card')).toHaveCount(7)
  const main = page.locator('.main.lumina-scrollbar')
  const styles = () =>
    main.evaluate((element) => ({
      gutter: getComputedStyle(element).scrollbarGutter,
      width: getComputedStyle(element, '::-webkit-scrollbar').width,
      radius: getComputedStyle(element, '::-webkit-scrollbar-thumb').borderRadius,
      track: getComputedStyle(element, '::-webkit-scrollbar-track').backgroundColor,
      thumb: getComputedStyle(element, '::-webkit-scrollbar-thumb').backgroundColor,
      buttons: getComputedStyle(element, '::-webkit-scrollbar-button').display,
    }))
  const light = await styles()
  expect(light).toMatchObject({
    gutter: 'stable',
    width: '10px',
    radius: '999px',
    track: 'rgba(0, 0, 0, 0)',
    buttons: 'none',
  })
  await page.evaluate(() => {
    document.documentElement.dataset.theme = 'dark'
  })
  const dark = await styles()
  expect(dark.thumb).not.toBe(light.thumb)
  const before = await main.evaluate((element) => element.clientWidth)
  await main.hover()
  await page.mouse.wheel(0, 500)
  await expect.poll(() => main.evaluate((element) => element.scrollTop)).toBeGreaterThan(0)
  expect(await main.evaluate((element) => element.clientWidth)).toBe(before)
  await page.emulateMedia({ forcedColors: 'active' })
  expect(await main.evaluate((element) => getComputedStyle(element).scrollbarColor)).toBe('auto')
})

test('missing capabilities fail closed without breaking the shell', async ({ page }) => {
  const errors: string[] = []
  page.on('pageerror', (error) => errors.push(error.message))
  await setup(page, 'admin', false)
  await page.route('**/api/v1/client/me/permissions', (route) =>
    route.fulfill({ json: { data: {}, request_id: 'missing-capabilities' } }),
  )
  const loaded = page.waitForResponse((response) =>
    response.url().endsWith('/client/me/permissions'),
  )
  await page.getByRole('button', { name: '刷新权限', exact: true }).click()
  await loaded
  await expect(page.locator('.page-heading h1')).toBeVisible()
  await expect(page.getByRole('link', { name: '机器性能' })).toHaveCount(0)
  await expect(page.getByRole('link', { name: '用户管理', exact: true })).toHaveCount(0)
  expect(errors).toEqual([])
})
