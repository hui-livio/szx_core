import { afterEach, describe, expect, it, vi } from 'vitest'
import { alignLuminaPopupArrow } from '../../src/ui/lumina/utils/popupArrow'

afterEach(() => vi.unstubAllGlobals())

describe('Lumina popup arrow positioning', () => {
  it.each([
    { name: 'centered', width: 800, anchorLeft: 200, popupLeft: 200, expectedLeft: 140, x: 80 },
    { name: 'left edge', width: 390, anchorLeft: 8, popupLeft: 8, expectedLeft: 8, x: 20 },
    { name: 'right edge', width: 390, anchorLeft: 342, popupLeft: 222, expectedLeft: 222, x: 140 },
  ])('prefers centering, only falling back at the $name', (testCase) => {
    vi.stubGlobal('document', { documentElement: { clientWidth: testCase.width } })
    const properties: Record<string, string> = {}
    const style = {
      translate: '',
      marginTop: '',
      setProperty: (name: string, value: string) => {
        properties[name] = value
      },
    }
    const dataset: Record<string, string> = {}
    const classes = new Set<string>()
    const popup = {
      getBoundingClientRect: () => ({ left: testCase.popupLeft, top: 100, width: 160 }),
      style,
      dataset,
      classList: { add: (value: string) => classes.add(value) },
    } as unknown as HTMLElement
    const anchor = {
      getBoundingClientRect: () => ({ left: testCase.anchorLeft, top: 50, width: 40 }),
    } as HTMLElement
    alignLuminaPopupArrow(popup, anchor)
    expect(style.translate).toBe(`${testCase.expectedLeft - testCase.popupLeft}px 0`)
    expect(properties['--lumina-popup-anchor-x']).toBe(`${testCase.x}px`)
    expect(dataset.side).toBe('bottom')
    expect(style.marginTop).toBe('var(--lg-s2)')
    expect(classes.has('lumina-popup-arrow')).toBe(true)
  })

  it('flips the triangle and gutter when collision handling puts the popup above', () => {
    vi.stubGlobal('document', { documentElement: { clientWidth: 800 } })
    const popup = {
      getBoundingClientRect: () => ({ left: 200, top: 100, width: 160 }),
      style: { translate: '', marginTop: '', setProperty: vi.fn() },
      dataset: {},
      classList: { add: vi.fn() },
    } as unknown as HTMLElement
    const anchor = {
      getBoundingClientRect: () => ({ left: 200, top: 300, width: 40 }),
    } as HTMLElement
    alignLuminaPopupArrow(popup, anchor)
    expect(popup.dataset.side).toBe('top')
    expect(popup.style.marginTop).toBe('calc(-1 * var(--lg-s2))')
  })
})
