import { expect, type Page } from '@playwright/test'

/** The same material tokens are used by login, page panels and performance cards. */
export async function expectThemedPanels(page: Page, selector: string) {
  const panels = page.locator(selector)
  await expect(panels.first()).toBeVisible()
  const backgrounds = new Set<string>()
  for (const theme of ['light', 'dark']) {
    for (const effects of ['lite', 'full']) {
      await page.evaluate(
        ({ theme, effects }) => {
          document.documentElement.dataset.theme = theme
          document.documentElement.dataset.fx = effects
        },
        { theme, effects },
      )
      const expected = await page.evaluate(() => {
        const probe = document.createElement('span')
        probe.style.backgroundColor = 'var(--lg-panel-background)'
        document.body.append(probe)
        const color = getComputedStyle(probe).backgroundColor
        probe.remove()
        return color
      })
      const states = await panels.evaluateAll((elements) =>
        elements.map((element) => {
          const css = getComputedStyle(element)
          return {
            shared: element.classList.contains('lumina-panel-glass'),
            background: css.backgroundColor,
            highlight: css.backgroundImage,
            filter: css.backdropFilter,
            radius: parseFloat(css.borderBottomLeftRadius),
          }
        }),
      )
      for (const state of states) {
        expect(state.shared).toBe(true)
        expect(state.background).toBe(expected)
        expect(state.highlight).toContain('linear-gradient')
        expect(state.filter).toBe('none')
        expect(state.radius).toBeGreaterThan(0)
        backgrounds.add(state.background)
      }
      if (theme === 'dark' && effects === 'full') {
        await page.screenshot({
          path: `test-results/themed-panel-${new URL(page.url()).pathname.replaceAll('/', '-')}.png`,
        })
      }
    }
  }
  expect(backgrounds.size).toBe(4)
  await page.evaluate(() => {
    document.documentElement.dataset.theme = 'light'
    document.documentElement.dataset.fx = 'lite'
  })
}
