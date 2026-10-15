import { expect, type Locator } from '@playwright/test'

// Scroll real body controls into the same physical area as the sticky header,
// then verify painting/hit testing: geometry overlap is expected during scrolling.
export async function expectHeaderAboveRowControls(scroller: Locator) {
  await expect.poll(() => scroller.locator('tbody tr').count()).toBeGreaterThan(1)
  const page = scroller.page()
  await page.evaluate(() => document.fonts.ready)
  let checked = 0
  for (const column of ['first', 'last'] as const) {
    const candidate = await scroller.evaluate((element, column) => {
      element.scrollTop = 0
      element.scrollLeft = column === 'first' ? 0 : element.scrollWidth
      const cells = [...element.querySelectorAll<HTMLTableCellElement>('tbody tr:first-child > td')]
      const cell = column === 'first' ? cells[0] : cells.at(-1)
      const viewport = element.getBoundingClientRect()
      const control = [...(cell?.querySelectorAll<HTMLElement>('button, a, input') ?? [])].find(
        (item) => {
          const rect = item.getBoundingClientRect()
          return rect.right > viewport.left + 1 && rect.left < viewport.right - 16
        },
      )
      if (!control) return false
      const container = element.getBoundingClientRect()
      const header = element.querySelector('thead')!.getBoundingClientRect()
      const rect = control.getBoundingClientRect()
      element.scrollTop = rect.top + rect.height / 2 - container.top - header.height / 2
      return true
    }, column)
    if (!candidate) continue
    checked++
    // Let the browser complete scroll/paint before inspecting the rendered layer.
    await page.evaluate(
      () => new Promise<void>((resolve) => requestAnimationFrame(() => resolve())),
    )
    const result = await scroller.evaluate((element, column) => {
      const head = element.querySelector('thead')!
      const header = head.getBoundingClientRect()
      const container = element.getBoundingClientRect()
      const cells = [...element.querySelectorAll<HTMLTableCellElement>('tbody tr:first-child > td')]
      const cell = column === 'first' ? cells[0] : cells.at(-1)
      const control = [...cell!.querySelectorAll<HTMLElement>('button, a, input')].find((item) => {
        const rect = item.getBoundingClientRect()
        return rect.right > container.left + 1 && rect.left < container.right - 16
      })!
      const rect = control.getBoundingClientRect()
      const left = Math.max(rect.left, container.left + 1)
      const right = Math.min(rect.right, container.right - 16)
      const top = Math.max(rect.top, header.top + 1)
      const bottom = Math.min(rect.bottom, header.bottom - 1)
      const point = document.elementFromPoint((left + right) / 2, (top + bottom) / 2)
      return {
        scrollTop: element.scrollTop,
        headerTop: header.top,
        containerTop: container.top,
        overlapWidth: right - left,
        overlapHeight: bottom - top,
        headerOwnsPoint: Boolean(point && head.contains(point)),
        rowOwnsPoint: Boolean(point && element.querySelector('tbody')?.contains(point)),
        cellOffsets: [...head.querySelectorAll('th')].map((cell) =>
          Math.abs(cell.getBoundingClientRect().top - header.top),
        ),
      }
    }, column)
    expect(result.scrollTop).toBeGreaterThan(0)
    expect(Math.abs(result.headerTop - result.containerTop)).toBeLessThan(2)
    expect(result.overlapWidth).toBeGreaterThan(0)
    expect(result.overlapHeight).toBeGreaterThan(0)
    expect(result.headerOwnsPoint).toBe(true)
    expect(result.rowOwnsPoint).toBe(false)
    for (const offset of result.cellOffsets) expect(offset).toBeLessThan(2)
  }
  expect(checked).toBeGreaterThan(0)
  await scroller.evaluate((element) => {
    element.scrollTop = 0
    element.scrollLeft = 0
  })
}
