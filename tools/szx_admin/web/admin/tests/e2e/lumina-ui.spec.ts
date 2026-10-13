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
      .getByRole('heading')
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
