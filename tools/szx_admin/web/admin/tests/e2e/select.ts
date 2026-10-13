import type { Locator, Page } from '@playwright/test'

export async function chooseOption(page: Page, control: Locator, label: string) {
  await control.click()
  await page.getByRole('option', { name: label, exact: true }).click()
}
