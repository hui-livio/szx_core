import { onBeforeUnmount, onMounted } from 'vue'

import { onDevicePixelRatioChange } from '../theme/appearance'

// Admin forms and date pickers retain the existing readability floor. Unlike the
// client dashboard, ordinary pages scroll instead of shrinking to fit short screens.
export function computeLuminaScale(width: number, height: number): number {
  const base = Math.min(width / 1440, height / 900)
  return Math.max(0.92, Math.min(1.5, 1 + (base - 1) * 0.7))
}

export function snapLuminaPixels(length: number, devicePixelRatio = 1): number {
  const ratio = Number.isFinite(devicePixelRatio) && devicePixelRatio > 0 ? devicePixelRatio : 1
  return Math.round(length * ratio) / ratio
}

export function useLuminaScale() {
  let frame = 0

  const apply = () => {
    frame = 0
    const ratio = window.devicePixelRatio || 1
    const scale = Number(computeLuminaScale(window.innerWidth, window.innerHeight).toFixed(2))
    const root = document.documentElement.style
    root.setProperty('--k', scale.toFixed(2))
    // Align small circular controls and their SVG viewport to physical pixels.
    root.setProperty('--lg-control-px', `${snapLuminaPixels(42 * scale, ratio)}px`)
    root.setProperty('--lg-icon-px', `${snapLuminaPixels(20 * scale, ratio)}px`)
  }

  /// 拖拽窗口边缘会连续抛几十次 resize，逐次写 CSS 变量等于让全页重排几十遍 ——
  /// 同一帧只算一次
  const schedule = () => {
    if (frame) {
      return
    }
    frame = requestAnimationFrame(apply)
  }

  const disposeDpr = onDevicePixelRatioChange(schedule)
  const viewport = typeof window === 'undefined' ? undefined : window.visualViewport

  onMounted(() => {
    apply()
    window.addEventListener('resize', schedule)
    window.addEventListener('orientationchange', schedule)
    viewport?.addEventListener('resize', schedule)
  })

  onBeforeUnmount(() => {
    disposeDpr()
    window.removeEventListener('resize', schedule)
    window.removeEventListener('orientationchange', schedule)
    viewport?.removeEventListener('resize', schedule)
    if (frame) {
      cancelAnimationFrame(frame)
    }
  })
}
