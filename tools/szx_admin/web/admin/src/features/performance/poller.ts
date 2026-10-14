/** Single-flight polling. Stop invalidates responses even if an adapter ignores AbortSignal. */
export function createMetricsPoller(
  fetch: (signal: AbortSignal, current: () => boolean) => Promise<void>,
  onError: (error: unknown) => void,
) {
  let active = false
  let generation = 0
  let inFlight = false
  let failures = 0
  let timer: ReturnType<typeof setTimeout> | undefined
  let controller: AbortController | undefined
  async function tick() {
    if (!active || inFlight) return
    inFlight = true
    const epoch = generation
    const abort = new AbortController()
    controller = abort
    const current = () => active && epoch === generation && !abort.signal.aborted
    try {
      await fetch(abort.signal, current)
      if (current()) failures = 0
    } catch (error) {
      if (current()) {
        failures++
        onError(error)
      }
    } finally {
      inFlight = false
      controller = undefined
      if (active)
        timer = setTimeout(
          () => void tick(),
          epoch !== generation ? 0 : Math.min(30_000, 2000 * 2 ** failures),
        )
    }
  }
  return {
    start() {
      if (active) return
      active = true
      generation++
      failures = 0
      if (!inFlight) void tick()
    },
    stop() {
      active = false
      generation++
      clearTimeout(timer)
      controller?.abort()
    },
  }
}
