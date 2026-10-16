import { test, expect } from '@playwright/test'

const fixture = '/tests/fixtures/lumina/index.html'

test('dialog opened busy regains Escape dismissal when loading finishes', async ({ page }) => {
  await page.goto(fixture)
  await page.getByRole('button', { name: 'Toggle busy', exact: true }).click()
  const opener = page.getByRole('button', { name: 'Open dialog', exact: true })
  await opener.click()
  const dialog = page.getByRole('dialog', { name: 'Reusable dialog', exact: true })
  await expect(dialog.getByRole('button', { name: 'Close', exact: true })).toBeDisabled()
  await page.keyboard.press('Escape')
  await expect(dialog).toBeVisible()
  await dialog.getByRole('button', { name: 'Toggle dialog busy', exact: true }).click()
  await expect(dialog.getByRole('button', { name: 'Close', exact: true })).toBeEnabled()
  await dialog.getByRole('textbox', { name: 'Dialog text', exact: true }).focus()
  await page.keyboard.press('Escape')
  await expect(dialog).toHaveCount(0)
  await expect(opener).toBeFocused()
})

test('one Escape closes only the top dialog and restores focus to its opener', async ({ page }) => {
  await page.goto(fixture)
  const opener = page.getByRole('button', { name: 'Open dialog', exact: true })
  await opener.click()
  const parent = page.getByRole('dialog', { name: 'Reusable dialog', exact: true })
  const nestedOpener = parent.getByRole('button', { name: 'Open nested dialog', exact: true })
  await nestedOpener.click()
  const nested = page.getByRole('dialog', { name: 'Nested dialog', exact: true })
  await nested.getByRole('textbox', { name: 'Nested text', exact: true }).focus()
  await page.keyboard.press('Escape')
  await expect(nested).toHaveCount(0)
  await expect(parent).toBeVisible()
  await expect(nestedOpener).toBeFocused()
  await page.keyboard.press('Escape')
  await expect(parent).toHaveCount(0)
  await expect(opener).toBeFocused()
})

for (const name of ['Dialog choice', 'Dialog editable choice', 'Dialog date']) {
  test(`Escape closes the open ${name} overlay first, then its dialog`, async ({ page }) => {
    await page.goto(fixture)
    await page.getByRole('button', { name: 'Open dialog', exact: true }).click()
    const dialog = page.getByRole('dialog', { name: 'Reusable dialog', exact: true })
    const control = dialog.getByRole('combobox', { name, exact: true })
    await control.focus()
    await control.press('ArrowDown')
    await expect(control).toHaveAttribute('aria-expanded', 'true')
    await page.keyboard.press('Escape')
    await expect(control).toHaveAttribute('aria-expanded', 'false')
    await expect(dialog).toBeVisible()
    await control.focus()
    await page.keyboard.press('Escape')
    await expect(dialog).toHaveCount(0)
  })
}

test('Escape in a calendar portal preserves the parent and next Escape closes it', async ({
  page,
}) => {
  await page.goto(fixture)
  await page.getByRole('button', { name: 'Open dialog', exact: true }).click()
  const dialog = page.getByRole('dialog', { name: 'Reusable dialog', exact: true })
  const control = dialog.getByRole('combobox', { name: 'Dialog date', exact: true })
  await control.press('ArrowDown')
  const calendar = page.locator('.lumina-datetime-panel')
  await expect(calendar).toBeVisible()
  await calendar.getByRole('button', { name: 'Next Month', exact: true }).focus()
  await page.keyboard.press('Escape')
  await expect(calendar).toHaveCount(0)
  await expect(dialog).toBeVisible()
  await page.keyboard.press('Escape')
  await expect(dialog).toHaveCount(0)
})

for (const movedFocus of [false, true]) {
  test(`menu consumes one Escape before its parent (focus moved: ${movedFocus})`, async ({
    page,
  }) => {
    await page.goto(fixture)
    await page.getByRole('button', { name: 'Open dialog', exact: true }).click()
    const dialog = page.getByRole('dialog', { name: 'Reusable dialog', exact: true })
    await dialog.getByRole('button', { name: 'Dialog popup', exact: true }).click()
    const menu = page.getByRole('menu', { name: 'Dialog popup', exact: true })
    await expect(menu).toBeVisible()
    if (movedFocus) await dialog.getByRole('textbox', { name: 'Dialog text', exact: true }).focus()
    await page.keyboard.press('Escape')
    await expect(menu).toHaveCount(0)
    await expect(dialog).toBeVisible()
    await page.keyboard.press('Escape')
    await expect(dialog).toHaveCount(0)
  })
}

test('busy changes block Escape and close button, while composition and mask clicks never dismiss', async ({
  page,
}) => {
  await page.goto(fixture)
  await page.getByRole('button', { name: 'Open dialog', exact: true }).click()
  const dialog = page.getByRole('dialog', { name: 'Reusable dialog', exact: true })
  await expect(dialog.locator('.lumina-dialog-panel.lumina-panel-solid')).toBeVisible()
  const text = dialog.getByRole('textbox', { name: 'Dialog text', exact: true })
  await text.dispatchEvent('keydown', { key: 'Escape', code: 'Escape', isComposing: true })
  await expect(dialog).toBeVisible()
  await page.locator('.p-dialog-mask').click({ position: { x: 2, y: 2 } })
  await expect(dialog).toBeVisible()
  await dialog.getByRole('button', { name: 'Toggle dialog busy', exact: true }).click()
  await expect(dialog).toHaveAttribute('aria-busy', 'true')
  await text.focus()
  await page.keyboard.press('Escape')
  await expect(dialog).toBeVisible()
  await expect(dialog.getByRole('button', { name: 'Close', exact: true })).toBeDisabled()
  await dialog.getByRole('button', { name: 'Toggle dialog busy', exact: true }).click()
  await page.keyboard.press('Escape')
  await expect(dialog).toHaveCount(0)
  // A later dialog must have a fresh registration and a fresh focus target.
  const opener = page.getByRole('button', { name: 'Open dialog', exact: true })
  await opener.click()
  await page.keyboard.press('Escape')
  await expect(dialog).toHaveCount(0)
  await expect(opener).toBeFocused()
})

for (const name of ['Dialog choice', 'Dialog date']) {
  test(`Escape dismisses an open ${name} after focus returns to the form`, async ({ page }) => {
    await page.goto(fixture)
    await page.getByRole('button', { name: 'Open dialog', exact: true }).click()
    const dialog = page.getByRole('dialog', { name: 'Reusable dialog', exact: true })
    const control = dialog.getByRole('combobox', { name, exact: true })
    await control.press('ArrowDown')
    await expect(control).toHaveAttribute('aria-expanded', 'true')
    await dialog.getByRole('textbox', { name: 'Dialog text', exact: true }).focus()
    await page.keyboard.press('Escape')
    await expect(control).toHaveAttribute('aria-expanded', 'false')
    await expect(dialog).toBeVisible()
    await page.keyboard.press('Escape')
    await expect(dialog).toHaveCount(0)
  })
}
