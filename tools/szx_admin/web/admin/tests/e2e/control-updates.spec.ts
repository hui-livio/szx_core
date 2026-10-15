import { expect, test, type Page } from '@playwright/test'

const fixture = '/tests/fixtures/lumina/updates.html'
async function settle(page: Page) {
  await page.evaluate(
    () =>
      new Promise<void>((resolve) =>
        requestAnimationFrame(() => requestAnimationFrame(() => resolve())),
      ),
  )
}
async function rememberRows(page: Page) {
  await page.locator('#update-table').evaluate((element) => {
    const component = (
      element as unknown as { __vueParentComponent: { props: { value: object[] } } }
    ).__vueParentComponent
    Object.defineProperty(component.props.value, '__updateTestBaseline', {
      value: true,
      configurable: true,
    })
    component.props.value.forEach((row) =>
      Object.defineProperty(row, '__updateTestRow', { value: true, configurable: true }),
    )
  })
}
async function stableRows(page: Page) {
  return page.locator('#update-table').evaluate((element) => {
    const component = (
      element as unknown as {
        __vueParentComponent: {
          props: {
            value: (object & { __updateTestRow?: boolean })[] & { __updateTestBaseline?: boolean }
          }
        }
      }
    ).__vueParentComponent
    return {
      array: Boolean(component.props.value.__updateTestBaseline),
      rows: component.props.value.map((row) => Boolean(row.__updateTestRow)),
    }
  })
}

test('equal replacement data preserves table values and skips chart setOption', async ({
  page,
}) => {
  await page.goto(fixture)
  const chart = page.locator('#update-chart')
  await expect(chart.locator('svg')).toHaveCount(1)
  await expect(chart).toHaveAttribute('data-updates', '0')
  await rememberRows(page)
  for (let i = 0; i < 4; i++)
    await page.getByRole('button', { name: 'Equivalent data', exact: true }).click()
  await settle(page)
  expect(await stableRows(page)).toEqual({ array: true, rows: [true, true] })
  await expect(chart).toHaveAttribute('data-updates', '0')
  await page.getByRole('button', { name: 'Beta', exact: true }).click()
  await expect(page.getByTestId('picked')).toHaveText('b:Beta')
})

test('actual and in-place data changes update the table and chart', async ({ page }) => {
  await page.goto(fixture)
  const chart = page.locator('#update-chart')
  await expect(chart).toHaveAttribute('data-updates', '0')
  await rememberRows(page)
  await page.getByRole('button', { name: 'Changed data', exact: true }).click()
  await expect(chart).toHaveAttribute('data-updates', '1')
  expect(await stableRows(page)).toEqual({ array: false, rows: [false, true] })
  await expect(page.locator('#update-table tbody tr').first()).toContainText('Changed')
  await page.getByRole('button', { name: 'Mutate data', exact: true }).click()
  await expect(chart).toHaveAttribute('data-updates', '2')
  await page.getByRole('button', { name: 'Mutated', exact: true }).click()
  await expect(page.getByTestId('picked')).toHaveText('a:Mutated')
  await expect(page.locator('#update-table tbody tr').first()).toContainText('false')
})

test('reordering, add/remove, empty state, localized columns, loading and sorting remain live', async ({
  page,
}) => {
  await page.goto(fixture)
  const rows = page.locator('#update-table tbody tr')
  await page.getByRole('button', { name: 'Equivalent data', exact: true }).click()
  await page.getByRole('button', { name: 'Reverse rows', exact: true }).click()
  await expect(rows.first()).toContainText('Beta')
  await page.getByRole('button', { name: 'Add row', exact: true }).click()
  await expect(rows).toHaveCount(3)
  await page.getByRole('button', { name: 'Remove row', exact: true }).click()
  await expect(rows).toHaveCount(2)
  await expect(rows.first()).toContainText('Alpha')
  await page.getByRole('button', { name: 'Rename column', exact: true }).click()
  await expect(
    page.getByRole('columnheader', { name: 'Localized name', exact: true }),
  ).toBeVisible()
  await page.getByRole('columnheader', { name: 'Localized name', exact: true }).click()
  await expect(rows.first()).toContainText('Alpha')
  await page.getByRole('columnheader', { name: 'Localized name', exact: true }).click()
  await expect(rows.first()).toContainText('Gamma')
  await page.getByRole('button', { name: 'Toggle busy', exact: true }).click()
  await expect(page.locator('#update-table')).toHaveAttribute('aria-busy', 'true')
  await page.getByRole('button', { name: 'Clear rows', exact: true }).click()
  await expect(page.getByText('No rows', { exact: true })).toBeVisible()
})

test('callback, theme, effects and resize changes are not suppressed by equal chart data', async ({
  page,
}) => {
  await page.goto(fixture)
  const chart = page.locator('#update-chart')
  await expect(chart).toHaveAttribute('data-updates', '0')
  await page.getByRole('button', { name: 'Change formatter', exact: true }).click()
  await expect(chart).toHaveAttribute('data-updates', '1')
  await page.getByRole('button', { name: 'Rename chart', exact: true }).click()
  await expect(chart).toHaveAttribute('aria-label', 'Renamed chart')
  await expect(chart).toHaveAttribute('data-updates', '1')
  await page.getByRole('button', { name: 'Toggle theme', exact: true }).click()
  await expect(chart).toHaveAttribute('data-updates', '2')
  await page.getByRole('button', { name: 'Toggle effects', exact: true }).click()
  await expect(chart).toHaveAttribute('data-updates', '3')
  const before = Number(await chart.getAttribute('data-resizes'))
  await page.locator('#chart-container').evaluate((element) => {
    element.style.width = '450px'
  })
  await expect
    .poll(async () => Number(await chart.getAttribute('data-resizes')))
    .toBeGreaterThan(before)
  await expect.poll(() => chart.locator('svg').getAttribute('width')).toBe('450')
})
