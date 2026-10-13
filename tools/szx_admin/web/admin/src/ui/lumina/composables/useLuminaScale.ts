import { onBeforeUnmount, onMounted } from 'vue'

export function computeLuminaScale(width: number, height: number): number {
  const base = Math.min(width / 1440, height / 900)
  return Math.max(0.92, Math.min(1.5, 1 + (base - 1) * 0.7))
}

export function snapLuminaPixels(length: number, devicePixelRatio = 1): number {
  const ratio = Number.isFinite(devicePixelRatio) && devicePixelRatio > 0 ? devicePixelRatio : 1
  return Math.round(length * ratio) / ratio
}

export function useLuminaScale() {
  const update = () => {
    const scale = computeLuminaScale(window.innerWidth, window.innerHeight)
    const root = document.documentElement.style
    root.setProperty('--k', scale.toFixed(2))
    // Align small circular controls and their SVG viewport to physical pixels.
    const displayedScale = Number(scale.toFixed(2))
    root.setProperty(
      '--lg-control-px',
      `${snapLuminaPixels(42 * displayedScale, window.devicePixelRatio)}px`,
    )
    root.setProperty(
      '--lg-icon-px',
      `${snapLuminaPixels(20 * displayedScale, window.devicePixelRatio)}px`,
    )
  }
  onMounted(() => {
    update()
    window.addEventListener('resize', update)
  })
  onBeforeUnmount(() => window.removeEventListener('resize', update))
}
