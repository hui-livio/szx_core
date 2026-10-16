import { afterEach, describe, expect, it, vi } from 'vitest'
import { createMetricsPoller } from '../src/features/performance/poller'
import { formatBytes, mergeSamples } from '../src/features/performance/model'
import type { MachineSample } from '../src/api/types'

const sample = (sequence: number, time = sequence * 2000) =>
  ({ sequence, sampled_at: time }) as MachineSample

describe('metrics history', () => {
  it('deduplicates by sequence and bounds rolling history to 450 samples', () => {
    const result = mergeSamples(
      Array.from({ length: 500 }, (_, index) => sample(index)),
      [sample(499), sample(500)],
    )
    expect(result).toHaveLength(450)
    expect(result[0]?.sequence).toBe(51)
    expect(result.at(-1)?.sequence).toBe(500)
  })
  it('excludes samples older than 15 minutes', () => {
    expect(
      mergeSamples([sample(1, 0)], [sample(2, 901_000)]).map((value) => value.sequence),
    ).toEqual([2])
  })
  it.each([
    [null, '—'],
    [0, '0 B'],
    [1024, '1 KiB'],
    [1073741824, '1 GiB'],
    [-1, '—'],
  ])('formats %s as %s', (input, expected) => {
    expect(formatBytes(input as number | null, 'en-US')).toBe(expected)
  })
})

describe('metrics single-flight poller', () => {
  afterEach(() => {
    vi.useRealTimers()
  })
  it('never overlaps requests and stops scheduling after stop', async () => {
    vi.useFakeTimers()
    let finish!: () => void
    const fetch = vi.fn(
      () =>
        new Promise<void>((resolve) => {
          finish = resolve
        }),
    )
    const poller = createMetricsPoller(fetch, vi.fn())
    poller.start()
    poller.start()
    await vi.advanceTimersByTimeAsync(20_000)
    expect(fetch).toHaveBeenCalledTimes(1)
    finish()
    await vi.advanceTimersByTimeAsync(2000)
    expect(fetch).toHaveBeenCalledTimes(2)
    poller.stop()
    finish()
    await vi.advanceTimersByTimeAsync(40_000)
    expect(fetch).toHaveBeenCalledTimes(2)
  })
  it('invalidates stale responses even if cancellation is ignored and resumes single-flight', async () => {
    vi.useFakeTimers()
    let finish!: () => void
    let committed = 0
    const signals: AbortSignal[] = []
    const fetch = vi.fn(async (signal: AbortSignal, current: () => boolean) => {
      signals.push(signal)
      await new Promise<void>((resolve) => {
        finish = resolve
      })
      if (current()) committed++
    })
    const poller = createMetricsPoller(fetch, vi.fn())
    poller.start()
    poller.stop()
    poller.start()
    expect(signals[0]?.aborted).toBe(true)
    expect(fetch).toHaveBeenCalledTimes(1)
    finish()
    await vi.advanceTimersByTimeAsync(0)
    expect(committed).toBe(0)
    expect(fetch).toHaveBeenCalledTimes(2)
    finish()
    await vi.advanceTimersByTimeAsync(0)
    expect(committed).toBe(1)
    poller.stop()
  })
  it('backs off on errors and resets after a successful sample', async () => {
    vi.useFakeTimers()
    let failures = true
    const fetch = vi.fn(async () => {
      if (failures) throw new Error('offline')
    })
    const onError = vi.fn()
    const poller = createMetricsPoller(fetch, onError)
    poller.start()
    await vi.advanceTimersByTimeAsync(0)
    expect(onError).toHaveBeenCalledTimes(1)
    await vi.advanceTimersByTimeAsync(3999)
    expect(fetch).toHaveBeenCalledTimes(1)
    failures = false
    await vi.advanceTimersByTimeAsync(1)
    expect(fetch).toHaveBeenCalledTimes(2)
    await vi.advanceTimersByTimeAsync(2000)
    expect(fetch).toHaveBeenCalledTimes(3)
    poller.stop()
  })
})
