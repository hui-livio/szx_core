import { onBeforeUnmount, onMounted, type Ref } from 'vue'
import {
  backgroundParticle,
  backgroundParticleCount,
  backgroundSpriteLimit,
  LUMINA_BACKGROUND_FPS,
  LUMINA_BACKGROUND_ORB_PIXELS,
} from '../utils/background'
import { onDevicePixelRatioChange } from '../theme/appearance'

/** A single, budgeted clock for all sprites. No Vue rendering or layout reads per frame. */
export function useLuminaBackground(host: Ref<HTMLElement | undefined>) {
  let orbs: HTMLElement[] = []
  let sparks: HTMLElement[] = []
  let spot: HTMLElement | null = null
  let appearance: MutationObserver | undefined
  let motion: MediaQueryList | undefined
  let pointer: MediaQueryList | undefined
  let disposeDpr: (() => void) | undefined
  let timer: ReturnType<typeof setTimeout> | undefined
  let frame: number | undefined
  let active = false
  let tracking = false
  let width = 0
  let height = 0
  let count = 0
  let elapsed = 0
  let startedAt = 0
  let lastFrame = 0
  let point: { x: number; y: number } | undefined
  const interval = 1000 / LUMINA_BACKGROUND_FPS

  function track(event: PointerEvent) {
    // Input events only replace one pending point; the shared clock consumes it.
    if (event.pointerType === 'mouse' || event.pointerType === 'pen') {
      point = { x: event.clientX, y: event.clientY }
    }
  }

  function syncPointer() {
    const enabled = active && !!pointer?.matches
    if (enabled === tracking) return
    tracking = enabled
    host.value?.toggleAttribute('data-pointer', enabled)
    if (enabled) window.addEventListener('pointermove', track, { passive: true })
    else {
      window.removeEventListener('pointermove', track)
      point = undefined
    }
  }

  function measure() {
    width = window.innerWidth
    height = window.innerHeight
    count = backgroundParticleCount(width)
    if (spot && !spot.style.transform) {
      spot.style.transform = `translate3d(${width / 2}px,${height * 0.4}px,0)`
    }
    const ratio = window.devicePixelRatio
    const style = host.value?.style
    style?.setProperty(
      '--lg-orb-cap',
      `${backgroundSpriteLimit(480, LUMINA_BACKGROUND_ORB_PIXELS, ratio)}px`,
    )
    style?.setProperty('--lg-spot-cap', `${backgroundSpriteLimit(280, 448, ratio)}px`)
    sparks.forEach((spark, index) => {
      spark.hidden = index >= count
    })
  }

  function schedule() {
    // Wait outside rAF: a 120/144 Hz display must not wake this clock on every refresh.
    timer = setTimeout(
      () => {
        timer = undefined
        frame = requestAnimationFrame(draw)
      },
      Math.max(0, Math.ceil(interval - (performance.now() - lastFrame))),
    )
  }

  function draw(now: number) {
    frame = undefined
    if (!active) return
    // Guard against early timers and clock rounding: never publish above the budget.
    if (now - lastFrame >= interval) {
      lastFrame = now
      const seconds = (elapsed + now - startedAt) / 1000
      const amplitude = Math.min(width * 0.04, 64)
      orbs.forEach((orb, index) => {
        const phase = seconds / (4.5 + index) + index * 2
        const x = Math.sin(phase) * amplitude
        const y = Math.cos(phase * 0.8) * amplitude
        // Translation only: scaling a gradient can cause texture re-rasterization.
        orb.style.transform = `translate3d(${x.toFixed(2)}px,${y.toFixed(2)}px,0)`
      })
      for (let index = 0; index < count; index++) {
        const particle = backgroundParticle(index, seconds, height)
        const spark = sparks[index]!
        spark.style.transform = `translate3d(0,${particle.y.toFixed(2)}px,0)`
        spark.style.opacity = particle.opacity.toFixed(3)
      }
      if (point && spot) {
        // The spot has a -50% static offset, so the input coordinate is its centre.
        spot.style.transform = `translate3d(${point.x}px,${point.y}px,0)`
        point = undefined
      }
    }
    schedule()
  }

  function stop() {
    if (active) elapsed += performance.now() - startedAt
    active = false
    if (timer !== undefined) clearTimeout(timer)
    if (frame !== undefined) cancelAnimationFrame(frame)
    timer = undefined
    frame = undefined
    syncPointer()
  }

  function sync() {
    const enabled =
      document.documentElement.dataset.fx === 'full' && !document.hidden && !motion?.matches
    host.value?.setAttribute('data-motion', enabled ? 'running' : 'paused')
    if (!enabled) {
      stop()
      return
    }
    if (!active) {
      active = true
      startedAt = performance.now()
      lastFrame = startedAt - interval
      schedule()
    }
    syncPointer()
  }

  onMounted(() => {
    if (!host.value) return
    orbs = [...host.value.querySelectorAll<HTMLElement>('.lg-orb')]
    sparks = [...host.value.querySelectorAll<HTMLElement>('.lg-spark')]
    spot = host.value.querySelector<HTMLElement>('.lg-spot')
    motion = window.matchMedia('(prefers-reduced-motion: reduce)')
    pointer = window.matchMedia('(pointer: fine)')
    motion.addEventListener('change', sync)
    pointer.addEventListener('change', syncPointer)
    document.addEventListener('visibilitychange', sync)
    window.addEventListener('resize', measure, { passive: true })
    disposeDpr = onDevicePixelRatioChange(measure)
    appearance = new MutationObserver(sync)
    appearance.observe(document.documentElement, { attributes: true, attributeFilter: ['data-fx'] })
    measure()
    sync()
  })

  onBeforeUnmount(() => {
    stop()
    appearance?.disconnect()
    disposeDpr?.()
    motion?.removeEventListener('change', sync)
    pointer?.removeEventListener('change', syncPointer)
    document.removeEventListener('visibilitychange', sync)
    window.removeEventListener('resize', measure)
  })
}
