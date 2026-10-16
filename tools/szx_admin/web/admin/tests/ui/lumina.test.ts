import { describe, expect, it } from 'vitest'
import { readFileSync, readdirSync } from 'node:fs'
import { dirname, resolve } from 'node:path'
import { computeLuminaScale } from '../../src/ui/lumina/composables/useLuminaScale'
import { balancedCardRows } from '../../src/ui/lumina/utils/cardGrid'
import { createLuminaCalendarLocale } from '../../src/ui/lumina/theme/locale'

const root = resolve('src/ui/lumina')
const files = (directory: string): string[] =>
  readdirSync(directory, { withFileTypes: true }).flatMap((entry) =>
    entry.isDirectory() ? files(resolve(directory, entry.name)) : [resolve(directory, entry.name)],
  )

describe('Lumina source-module boundary', () => {
  it('has no application imports, i18n, router, Pinia or API dependency', () => {
    for (const path of files(root).filter((path) => /\.(ts|vue)$/.test(path))) {
      const source = readFileSync(path, 'utf8')
      const imports = [...source.matchAll(/(?:from\s+|import\s*)['"]([^'"]+)['"]/g)].map(
        (match) => match[1]!,
      )
      for (const imported of imports) {
        if (imported.startsWith('.'))
          expect(resolve(dirname(path), imported).startsWith(`${root}/`)).toBe(true)
        else expect(imported).toMatch(/^(vue$|echarts\/|primevue\/|@primevue\/|@primeuix\/)/)
      }
    }
  })
  it('application views consume the UI entry, not raw PrimeVue controls', () => {
    for (const path of [...files(resolve('src/components')), ...files(resolve('src/pages'))].filter(
      (path) => path.endsWith('.vue'),
    )) {
      expect(readFileSync(path, 'utf8')).not.toMatch(
        /from\s+['"](?:primevue|@primevue|@primeuix)\//,
      )
    }
  })

  it('all page panel shells reuse the same LuminaPanel material as login', () => {
    for (const path of files(resolve('src/pages')).filter((path) => path.endsWith('.vue'))) {
      const source = readFileSync(path, 'utf8')
      expect(source).not.toMatch(/<section[^>]*class="panel/)
      if (source.includes('class="panel')) expect(source).toContain('<LuminaPanel')
    }
    const metrics = readFileSync('src/pages/PerformancePage.vue', 'utf8')
    for (const card of metrics.matchAll(/<LuminaCard\s[\s\S]*?>/g)) {
      expect(card[0]).toContain('variant="glass"')
    }
  })
  it('keeps theme resources in the same directory without creating a package', () => {
    const names = files(root)
    expect(names).not.toContain(resolve(root, 'package.json'))
    expect(names).toContain(resolve(root, 'theme/glass/fonts/OFL-LICENSE.txt'))
    expect(names).toContain(resolve(root, 'theme/glass/fonts/ATTRIBUTION.txt'))
  })
})

it.each([
  [1440, 900, 1],
  [1366, 768, 0.92],
  [1920, 1080, 1.14],
  [2560, 1440, 1.42],
  [320, 480, 0.92],
  [10000, 10000, 1.5],
])('scales %s×%s without app-specific dependencies', (width, height, expected) => {
  expect(computeLuminaScale(width, height)).toBe(expected)
})

it.each(['zh-CN', 'en-US', 'ja-JP', 'ko-KR', 'fr-FR', 'ru-RU'])(
  'builds a caller-localized calendar for %s',
  (locale) => {
    const result = createLuminaCalendarLocale(locale, {
      today: 'Caller today',
      clear: 'Caller clear',
      chooseDate: 'Caller date',
      chooseMonth: 'Caller month',
      chooseYear: 'Caller year',
      navigation: { nextSecond: 'Caller second' },
    })
    expect(result.dayNames).toHaveLength(7)
    expect(result.monthNames).toHaveLength(12)
    expect(result.today).toBe('Caller today')
    expect(result.nextSecond).toBe('Caller second')
  },
)

it('applies theme and effect attributes independently without persisting browser state', async () => {
  const { applyLuminaAppearance } = await import('../../src/ui/lumina/theme/appearance')
  const target = { dataset: {} } as HTMLElement
  applyLuminaAppearance({ theme: 'dark', effects: 'full' }, target)
  expect(target.dataset).toEqual({ theme: 'dark', fx: 'full' })
  applyLuminaAppearance({ theme: 'light' }, target)
  expect(target.dataset).toEqual({ theme: 'light', fx: 'full' })
  expect(() => applyLuminaAppearance({ effects: 'lite' })).not.toThrow()
})

it.each([1, 1.25, 1.5, 2])('snaps small controls to physical pixels at DPR %s', async (ratio) => {
  const { snapLuminaPixels } = await import('../../src/ui/lumina/composables/useLuminaScale')
  const value = snapLuminaPixels(42 * 0.92, ratio)
  expect(value * ratio).toBeCloseTo(Math.round(42 * 0.92 * ratio), 8)
})

it.each([
  [12, '12px'],
  [-4, '0px'],
  ['50%', '50%'],
  ['var(--lg-r-lg)', 'var(--lg-r-lg)'],
] as const)('normalizes reusable control lengths: %s', async (input, expected) => {
  const { toCssLength } = await import('../../src/ui/lumina/utils/cssLength')
  expect(toCssLength(input)).toBe(expected)
})

it.each([
  [0, 4, []],
  [1, 5, [1]],
  [2, 5, [2]],
  [3, 5, [3]],
  [5, 4, [3, 2]],
  [6, 4, [3, 3]],
  [7, 5, [4, 3]],
  [7, 3, [3, 2, 2]],
  [8, 4, [4, 4]],
  [9, 4, [3, 3, 3]],
  [5, 2, [2, 2, 1]],
  [3, 1, [1, 1, 1]],
  [3, 0, [1, 1, 1]],
])('balances %s cards with capacity %s into %s', (count, capacity, expected) => {
  expect(balancedCardRows(count as number, capacity as number)).toEqual(expected)
})

it('preserves counts and bounds row imbalance for all supported metric counts', () => {
  for (let count = 1; count <= 34; count++) {
    for (let capacity = 1; capacity <= 10; capacity++) {
      const rows = balancedCardRows(count, capacity)
      expect(rows.reduce((sum, size) => sum + size, 0)).toBe(count)
      expect(Math.max(...rows)).toBeLessThanOrEqual(capacity)
      expect(Math.max(...rows) - Math.min(...rows)).toBeLessThanOrEqual(1)
      expect(rows).toHaveLength(Math.ceil(count / capacity))
    }
  }
})
