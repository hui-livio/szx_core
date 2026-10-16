import { expect, it } from 'vitest'
import { applyLuminaBrand, normalizeLuminaBrand } from '../../src/ui/lumina/theme/appearance'

function target() {
  const values = new Map<string, string>()
  const element = {
    dataset: {},
    style: {
      setProperty: (key: string, value: string) => values.set(key, value),
      removeProperty: (key: string) => values.delete(key),
    },
  } as unknown as HTMLElement
  return { element, values }
}

it.each(['red', '#fff', '#1234567', 'url(test)', '#xx1234', ''])(
  'rejects non-hex custom color %s',
  (value) => {
    expect(normalizeLuminaBrand(value)).toBeNull()
  },
)

it('normalizes colors and clears every override to restore the original palette', () => {
  const { element, values } = target()
  applyLuminaBrand('#12AB34', element)
  expect(element.dataset.brand).toBe('#12ab34')
  expect(values.get('--lg-brand-500')).toBe('#12ab34')
  expect([...values.keys()].filter((key) => /\d$/.test(key))).toHaveLength(10)
  for (const [key, value] of values) {
    if (/\d$/.test(key)) expect(value).toMatch(/^#[0-9a-f]{6}$/)
  }
  applyLuminaBrand(null, element)
  expect(element.dataset.brand).toBeUndefined()
  expect(values.size).toBe(0)
})

it.each([
  ['#ffff00', '#000000'],
  ['#ffffff', '#000000'],
  ['#000000', '#ffffff'],
  ['#003366', '#ffffff'],
])('chooses a readable foreground for custom brand %s', (color, contrast) => {
  const { element, values } = target()
  applyLuminaBrand(color, element)
  expect(values.get('--lg-brand-contrast')).toBe(contrast)
})
