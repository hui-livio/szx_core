import { test, expect } from '@playwright/test'

const fixture = '/tests/fixtures/lumina/index.html'

test('Lumina UI works without admin routes, Pinia, i18n or API calls', async ({ page }) => {
  const apiRequests: string[] = []
  page.on('request', (request) => {
    if (request.url().includes('/api/')) apiRequests.push(request.url())
  })
  await page.goto(fixture)
  await expect(page.getByRole('heading', { name: 'Standalone Lumina controls' })).toBeVisible()
  await page.getByLabel('Name').fill('Reusable')
  await expect(page.getByTestId('submit-icon')).toBeVisible()
  await page.getByRole('button', { name: 'Submit' }).click()
  await expect(page.getByTestId('submissions')).toHaveText('1')
  const select = page.getByRole('combobox', { name: 'Colour' })
  await expect(select).toHaveText('Shade: blue')
  await select.click()
  await page.getByRole('option', { name: 'Violet', exact: true }).click()
  await expect(select).toHaveText('Shade: violet')
  await expect(page.locator('.lumina-table tbody tr')).toHaveCount(2)
  await expect(page.getByRole('cell', { name: 'Row: Alpha' })).toBeVisible()
  await page.getByRole('button', { name: 'Next', exact: true }).click()
  await expect(page.getByRole('navigation', { name: 'Pages' })).toContainText('2 / 3')
  const opener = page.getByRole('button', { name: 'Open dialog' })
  await opener.click()
  await expect(page.getByTestId('dialog-footer')).toHaveText('Custom footer slot')
  await page.getByRole('dialog').getByRole('button', { name: 'Close' }).click()
  await expect(page.getByRole('dialog')).toHaveCount(0)
  await expect(opener).toBeFocused()
  expect(apiRequests).toEqual([])
  // Browser-interaction policy belongs to the consuming application, not the UI.
  expect(
    await page
      .getByRole('heading', { name: 'Standalone Lumina controls' })
      .evaluate((element) =>
        element.dispatchEvent(new MouseEvent('contextmenu', { bubbles: true, cancelable: true })),
      ),
  ).toBe(true)
})

test('standalone UI keeps opaque dark popups and the local-time model', async ({ page }) => {
  await page.goto(fixture)
  await page.evaluate(() => {
    document.documentElement.dataset.theme = 'dark'
  })
  const input = page.getByRole('combobox', { name: 'Local time' })
  await input.fill('2026-10-13 05:06:07')
  await expect(page.getByTestId('local-time-model')).toHaveText('2026-10-13T05:06:07')
  await input.press('ArrowDown')
  const popup = page.locator('.lumina-datetime-panel')
  await expect(popup).toBeVisible()
  expect(await popup.evaluate((element) => getComputedStyle(element).backgroundColor)).toBe(
    'rgb(20, 24, 51)',
  )
  await popup.getByRole('button', { name: 'Clear', exact: true }).click()
  await expect(input).toHaveValue('')
  await page.getByRole('button', { name: 'Toggle busy' }).click()
  await expect(page.getByRole('button', { name: 'Next', exact: true })).toBeDisabled()
  await page.getByRole('button', { name: 'Open dialog' }).click()
  const close = page.getByRole('dialog').getByRole('button', { name: 'Close' })
  await expect(close).toBeDisabled()
  await page.keyboard.press('Escape')
  await expect(page.getByRole('dialog')).toBeVisible()
})

test('shared logo and user marks honor size and configurable radius', async ({ page }) => {
  await page.goto(fixture)
  const shape = async (label: string) =>
    page.getByRole('img', { name: label }).evaluate((element) => {
      const rect = element.getBoundingClientRect()
      return {
        width: rect.width,
        height: rect.height,
        radius: getComputedStyle(element).borderRadius,
      }
    })
  expect(await shape('Reusable S mark')).toEqual({ width: 48, height: 48, radius: '6px' })
  expect(await shape('Reusable user mark')).toEqual({ width: 48, height: 48, radius: '50%' })
  expect(
    await page.getByRole('img', { name: 'Reusable S mark' }).evaluate((element) => ({
      size: getComputedStyle(element).fontSize,
      weight: getComputedStyle(element).fontWeight,
    })),
  ).toEqual({ size: '24px', weight: '400' })
  expect(
    await page
      .getByRole('img', { name: 'Reusable user mark' })
      .evaluate((element) => getComputedStyle(element).fontWeight),
  ).toBe('500')
})

test('generic panels switch glass effects without resizing and keep solid surfaces opaque', async ({
  page,
}) => {
  await page.goto(fixture)
  const glass = page.getByTestId('glass-panel')
  const solid = page.getByTestId('solid-panel')
  await expect(glass).toContainText('Reusable glass panel slot')
  expect(
    await glass.evaluate((element) => ({
      filter: getComputedStyle(element).backdropFilter,
      radius: getComputedStyle(element).borderRadius,
      padding: getComputedStyle(element).padding,
    })),
  ).toEqual({ filter: 'none', radius: '18px', padding: '16px' })
  expect(await solid.evaluate((element) => element.tagName)).toBe('SECTION')
  const before = await glass.boundingBox()
  await page.evaluate(() => {
    document.documentElement.dataset.fx = 'full'
  })
  expect(await glass.evaluate((element) => getComputedStyle(element).backdropFilter)).toContain(
    'blur(',
  )
  expect(await glass.boundingBox()).toEqual(before)
  expect(await solid.evaluate((element) => getComputedStyle(element).backgroundColor)).toBe(
    'rgb(255, 255, 255)',
  )
  expect(await solid.evaluate((element) => getComputedStyle(element).backdropFilter)).toBe('none')
  await page.evaluate(() => {
    document.documentElement.dataset.theme = 'dark'
  })
  expect(await solid.evaluate((element) => getComputedStyle(element).backgroundColor)).toBe(
    'rgb(20, 24, 51)',
  )
  await page.emulateMedia({ reducedMotion: 'reduce' })
  expect(await glass.evaluate((element) => getComputedStyle(element).backdropFilter)).toBe('none')
})

test('standalone Card slots/state and SVG Chart adapt to appearance changes', async ({ page }) => {
  await page.goto(fixture)
  const card = page.getByTestId('reusable-card')
  await expect(card.getByRole('heading', { name: 'Reusable card', exact: true })).toBeVisible()
  await expect(card.getByRole('button', { name: 'Card action' })).toBeVisible()
  await expect(card).toContainText('Caller body')
  await expect(card.locator('footer')).toHaveText('Caller footer')
  expect(await card.evaluate((element) => getComputedStyle(element).borderRadius)).toBe('18px')
  await expect(page.getByTestId('unavailable-card').getByRole('status')).toHaveText(
    'Caller unavailable',
  )
  const chart = card.getByRole('img', { name: 'Reusable chart' })
  await expect(chart.locator('svg')).toHaveCount(1)
  const chartId = await chart.getAttribute('_echarts_instance_')
  await page.evaluate(() => {
    document.documentElement.dataset.theme = 'dark'
    document.documentElement.dataset.fx = 'lite'
  })
  await page.emulateMedia({ reducedMotion: 'reduce' })
  await expect(chart.locator('svg')).toHaveCount(1)
  expect(await chart.getAttribute('_echarts_instance_')).toBe(chartId)
  expect(await card.evaluate((element) => getComputedStyle(element).backgroundColor)).not.toBe(
    'rgba(0, 0, 0, 0)',
  )
})

test('independent CardGrid balances changing counts and observes its container', async ({
  page,
}) => {
  await page.goto(fixture)
  const grid = page.getByTestId('adaptive-grid')
  const counts = () =>
    grid.evaluate((element) => {
      const rows: { top: number; count: number }[] = []
      for (const card of element.querySelectorAll('.lumina-card')) {
        const top = card.getBoundingClientRect().top
        const row = rows.at(-1)
        if (row && Math.abs(row.top - top) < 1) row.count++
        else rows.push({ top, count: 1 })
      }
      return rows.map((row) => row.count)
    })
  await expect.poll(counts).toEqual([3, 2, 2])
  await page.getByRole('button', { name: 'Remove grid card' }).click()
  await expect.poll(counts).toEqual([3, 3])
  await page.getByRole('button', { name: 'Toggle grid width' }).click()
  await expect.poll(counts).toEqual([1, 1, 1, 1, 1, 1])
})
