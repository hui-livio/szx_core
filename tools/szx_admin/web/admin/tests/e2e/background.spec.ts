import { test, expect, type Page } from '@playwright/test'

const fixture = '/tests/fixtures/lumina/background.html'

async function observeFrames(page: Page, duration = 400) {
  return page.locator('.lg-orb.a').evaluate(async (element, duration) => {
    let frames = 0
    const observer = new MutationObserver(() => frames++)
    observer.observe(element, { attributes: true, attributeFilter: ['style'] })
    const started = performance.now()
    await new Promise((resolve) => setTimeout(resolve, duration))
    observer.disconnect()
    return { frames, duration: performance.now() - started }
  }, duration)
}

test('full effects retain particles and halos within a 30 FPS budget', async ({ page }) => {
  await page.goto(fixture)
  const background = page.locator('.lg-bg')
  await expect(background).toHaveAttribute('data-motion', 'paused')
  expect((await observeFrames(page)).frames).toBe(0)
  await page.getByRole('button', { name: 'Toggle effects' }).click()
  await expect(background).toHaveAttribute('data-motion', 'running')
  await expect(page.locator('.lg-spark:visible')).toHaveCount(8)
  const sample = await observeFrames(page, 1800)
  expect(sample.frames).toBeGreaterThan(15)
  expect(sample.frames).toBeLessThanOrEqual(Math.ceil((sample.duration * 30) / 1000) + 1)
  const styles = await page
    .locator('.lg-orb, .lg-spark, .lg-spot, .lg-grid')
    .evaluateAll((elements) =>
      elements.map((element) => {
        const css = getComputedStyle(element)
        return { filter: css.filter, backdrop: css.backdropFilter, shadow: css.boxShadow }
      }),
    )
  expect(
    styles.every((s) => s.filter === 'none' && s.backdrop === 'none' && s.shadow === 'none'),
  ).toBe(true)
  await page.mouse.move(360, 240)
  await expect
    .poll(() => page.locator('.lg-spot').evaluate((e) => e.style.transform))
    .toContain('360px')
  await page.getByRole('button', { name: 'Toggle effects' }).click()
  await expect(background).toHaveAttribute('data-motion', 'paused')
  expect((await observeFrames(page)).frames).toBe(0)
  await expect(page.locator('.lg-spark:visible, .lg-spot:visible')).toHaveCount(0)
  expect(await page.locator('.lg-orb.a').evaluate((e) => getComputedStyle(e).willChange)).toBe(
    'auto',
  )
  const position = await page.locator('.lg-spot').evaluate((e) => e.style.transform)
  await page.mouse.move(700, 400)
  await page.waitForTimeout(100)
  expect(await page.locator('.lg-spot').evaluate((e) => e.style.transform)).toBe(position)
})

test('reduced motion, simulated background visibility and unmount stop the shared clock', async ({
  page,
}) => {
  await page.goto(fixture)
  await page.getByRole('button', { name: 'Toggle effects' }).click()
  await expect(page.locator('.lg-bg')).toHaveAttribute('data-motion', 'running')
  await page.emulateMedia({ reducedMotion: 'reduce' })
  await expect(page.locator('.lg-bg')).toHaveAttribute('data-motion', 'paused')
  expect((await observeFrames(page)).frames).toBe(0)
  await page.emulateMedia({ reducedMotion: 'no-preference' })
  await expect(page.locator('.lg-bg')).toHaveAttribute('data-motion', 'running')
  await page.evaluate(() => {
    Object.defineProperty(document, 'hidden', { configurable: true, value: true })
    document.dispatchEvent(new Event('visibilitychange'))
  })
  await expect(page.locator('.lg-bg')).toHaveAttribute('data-motion', 'paused')
  expect((await observeFrames(page)).frames).toBe(0)
  await page.evaluate(() => {
    Object.defineProperty(document, 'hidden', { configurable: true, value: false })
    document.dispatchEvent(new Event('visibilitychange'))
  })
  await expect(page.locator('.lg-bg')).toHaveAttribute('data-motion', 'running')
  // A detached sprite must not keep receiving updates from an orphan clock.
  await page.evaluate(() => {
    const node = document.querySelector('.lg-orb.a') as HTMLElement
    ;(window as unknown as { detached: HTMLElement }).detached = node
  })
  await page.getByRole('button', { name: 'Toggle background' }).click()
  await expect(page.locator('.lg-bg')).toHaveCount(0)
  const changes = await page.evaluate(async () => {
    const node = (window as unknown as { detached: HTMLElement }).detached
    let changes = 0
    const observer = new MutationObserver(() => changes++)
    observer.observe(node, { attributes: true })
    await new Promise((resolve) => setTimeout(resolve, 300))
    observer.disconnect()
    return changes
  })
  expect(changes).toBe(0)
  await page.getByRole('button', { name: 'Toggle background' }).click()
  await expect(page.locator('.lg-bg')).toHaveAttribute('data-motion', 'running')
  expect((await observeFrames(page)).frames).toBeGreaterThan(0)
})

for (const ratio of [1, 2, 3]) {
  test.describe(`physical sprite budget at DPR ${ratio}`, () => {
    test.use({ viewport: { width: 3840, height: 2160 }, deviceScaleFactor: ratio })
    test('caps raster edges on 4K and keeps particles on mobile resize', async ({ page }) => {
      await page.goto(fixture)
      await page.getByRole('button', { name: 'Toggle effects' }).click()
      const widths = await page
        .locator('.lg-orb')
        .evaluateAll((elements) =>
          elements.map((e) => e.getBoundingClientRect().width * devicePixelRatio),
        )
      expect(widths.every((width) => width <= 768.1)).toBe(true)
      await page.setViewportSize({ width: 390, height: 844 })
      await expect(page.locator('.lg-spark:visible')).toHaveCount(5)
      await page.screenshot({ path: `test-results/background-mobile-dpr-${ratio}.png` })
    })
  })
}

test.describe('touch-only input', () => {
  test.use({ hasTouch: true, isMobile: true, viewport: { width: 390, height: 844 } })
  test('does not allocate a mouse-follow halo', async ({ page }) => {
    await page.goto(fixture)
    await page.getByRole('button', { name: 'Toggle effects' }).click()
    await expect(page.locator('.lg-bg')).toHaveAttribute('data-motion', 'running')
    await expect(page.locator('.lg-bg')).not.toHaveAttribute('data-pointer', '')
    await expect(page.locator('.lg-spot')).toBeHidden()
    await expect(page.locator('.lg-spark:visible')).toHaveCount(5)
  })
})

test('admin keeps its original palette by default and persists an opt-in custom brand', async ({
  page,
}) => {
  await page.goto('/login')
  await expect(page.locator('html')).not.toHaveAttribute('data-brand')
  const brandColor = () =>
    page
      .locator('html')
      .evaluate((e) => getComputedStyle(e).getPropertyValue('--lg-brand-500').trim())
  expect(await brandColor()).toBe('#5b6cff')
  const customize = page.getByRole('button', { name: '自定义主题色', exact: true })
  await customize.click()
  const dialog = page.getByRole('dialog', { name: '自定义主题色', exact: true })
  await expect(dialog).toBeVisible()
  await dialog.getByLabel('主题色', { exact: true }).fill('#008866')
  await expect(page.locator('html')).toHaveAttribute('data-brand', '#008866')
  expect(await brandColor()).toBe('#008866')
  await dialog.getByRole('button', { name: '关闭', exact: true }).click()
  await expect(customize).toBeFocused()
  await page.reload()
  await expect(page.locator('html')).toHaveAttribute('data-brand', '#008866')
  await page.getByRole('switch', { name: '深色主题', exact: true }).click()
  expect(await brandColor()).toBe('#008866')
  await customize.click()
  await dialog.getByRole('button', { name: '恢复默认配色', exact: true }).click()
  await expect(page.locator('html')).not.toHaveAttribute('data-brand')
  expect(await brandColor()).toBe('#5b6cff')
  await dialog.getByRole('button', { name: '关闭', exact: true }).click()
  await page.reload()
  await expect(page.locator('html')).not.toHaveAttribute('data-brand')
})
