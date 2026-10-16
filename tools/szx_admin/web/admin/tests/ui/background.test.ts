import { describe, expect, it, vi, afterEach } from 'vitest'
import {
  backgroundParticle,
  backgroundParticleCount,
  backgroundSpriteLimit,
  LUMINA_BACKGROUND_FPS,
} from '../../src/ui/lumina/utils/background'
import { onDevicePixelRatioChange } from '../../src/ui/lumina/theme/appearance'

afterEach(() => vi.unstubAllGlobals())

describe('background animation budgets', () => {
  it('keeps the background clock separate from high-refresh UI rendering', () => {
    expect(LUMINA_BACKGROUND_FPS).toBe(30)
  })

  it.each([320, 720, 1440, 3840])('bounds particles at viewport width %s', (width) => {
    expect(backgroundParticleCount(width)).toBe(width <= 720 ? 5 : 8)
  })

  it.each([0.8, 1, 1.25, 1.5, 2, 3, 4])('caps physical sprite dimensions at DPR %s', (dpr) => {
    const size = backgroundSpriteLimit(480, 768, dpr)
    expect(size).toBeLessThanOrEqual(480)
    expect(size * dpr).toBeLessThanOrEqual(768)
  })

  it.each([0, -1, Number.NaN, Number.POSITIVE_INFINITY])('handles invalid DPR %s', (dpr) => {
    expect(backgroundSpriteLimit(480, 768, dpr)).toBe(480)
  })

  it('keeps deterministic particle paths and fades within bounds over long sessions', () => {
    for (const height of [480, 900, 2160]) {
      for (let index = 0; index < 8; index++) {
        for (const seconds of [0, 1, 17, 43, 86400, 864000]) {
          const value = backgroundParticle(index, seconds, height)
          expect(backgroundParticle(index, seconds, height)).toEqual(value)
          expect(value.y).toBeGreaterThanOrEqual(-20)
          expect(value.y).toBeLessThanOrEqual(height + 20)
          expect(value.opacity).toBeGreaterThanOrEqual(0)
          expect(value.opacity).toBeLessThanOrEqual(0.75)
        }
      }
    }
  })
})

it('rebinds the DPR listener and disposes its current subscription', () => {
  const queries: { listener?: () => void; removeEventListener: ReturnType<typeof vi.fn> }[] = []
  const matchMedia = vi.fn(() => {
    const query = {
      addEventListener: vi.fn((_type: string, listener: () => void) => {
        query.listener = listener
      }),
      removeEventListener: vi.fn(),
      listener: undefined as (() => void) | undefined,
    }
    queries.push(query)
    return query
  })
  const browser = { devicePixelRatio: 1, matchMedia }
  vi.stubGlobal('window', browser)
  const changed = vi.fn()
  const dispose = onDevicePixelRatioChange(changed)
  browser.devicePixelRatio = 2
  queries[0]!.listener!()
  expect(changed).toHaveBeenCalledOnce()
  expect(matchMedia).toHaveBeenLastCalledWith('(resolution: 2dppx)')
  expect(queries[0]!.removeEventListener).toHaveBeenCalledOnce()
  dispose()
  expect(queries[1]!.removeEventListener).toHaveBeenCalledOnce()
})
