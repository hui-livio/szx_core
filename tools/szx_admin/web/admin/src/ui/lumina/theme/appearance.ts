export type LuminaTheme = 'light' | 'dark'
export type LuminaThemePreference = 'system' | LuminaTheme
export type LuminaEffects = 'lite' | 'full'

export function systemPrefersDark(): boolean {
  return (
    typeof window !== 'undefined' &&
    typeof window.matchMedia === 'function' &&
    window.matchMedia('(prefers-color-scheme: dark)').matches
  )
}

export function resolveLuminaTheme(preference: LuminaThemePreference): LuminaTheme {
  return preference === 'system' ? (systemPrefersDark() ? 'dark' : 'light') : preference
}

export function onSystemThemeChange(listener: (dark: boolean) => void): () => void {
  if (typeof window === 'undefined' || typeof window.matchMedia !== 'function') return () => {}
  const query = window.matchMedia('(prefers-color-scheme: dark)')
  const update = (event: MediaQueryListEvent) => listener(event.matches)
  query.addEventListener('change', update)
  return () => query.removeEventListener('change', update)
}

/// 订阅「设备像素比变化」，返回退订函数
///
/// resize 事件不覆盖「把窗口拖到另一块显示器」和「浏览器缩放」这两种情况，
/// 而 `--lg-control-px` / `--lg-icon-px` 是按 DPR 取整的，不跟就会糊。
/// MediaQueryList 的 resolution 查询只能一次性匹配某个具体值，
/// 所以每次触发后都要用新的 DPR 重新挂一次
export function onDevicePixelRatioChange(listener: () => void): () => void {
  if (typeof window === 'undefined' || typeof window.matchMedia !== 'function') {
    return () => {}
  }

  let query: MediaQueryList | undefined
  const listen = () => {
    query?.removeEventListener('change', handler)
    const ratio = window.devicePixelRatio || 1
    query = window.matchMedia(`(resolution: ${ratio}dppx)`)
    query.addEventListener('change', handler)
  }
  const handler = () => {
    listener()
    listen()
  }

  listen()
  return () => query?.removeEventListener('change', handler)
}

export function applyLuminaAppearance(
  appearance: { theme?: LuminaTheme; effects?: LuminaEffects },
  target?: HTMLElement,
) {
  const root = target ?? (typeof document === 'undefined' ? undefined : document.documentElement)
  if (!root) return
  if (appearance.theme) root.dataset.theme = appearance.theme
  if (appearance.effects) root.dataset.fx = appearance.effects
}

export const LUMINA_DEFAULT_BRAND = '#5b6cff'

export function normalizeLuminaBrand(value: string): string | null {
  return /^#[0-9a-f]{6}$/i.test(value) ? value.toLowerCase() : null
}

/** Change only theme tokens; clearing an override restores the shipped palette exactly. */
export function applyLuminaBrand(value: string | null, target?: HTMLElement) {
  const root = target ?? (typeof document === 'undefined' ? undefined : document.documentElement)
  if (!root) return
  const color = value === null ? null : normalizeLuminaBrand(value)
  if (!color) {
    delete root.dataset.brand
    for (const stop of [50, 100, 200, 300, 400, 500, 600, 700, 800, 900]) {
      root.style.removeProperty(`--lg-brand-${stop}`)
    }
    root.style.removeProperty('--lg-brand-contrast')
    root.style.removeProperty('--lg-brand-hover-contrast')
    return
  }
  const rgb = [1, 3, 5].map((offset) => Number.parseInt(color.slice(offset, offset + 2), 16))
  const mix = (amount: number, target: number) =>
    `#${rgb
      .map((channel) =>
        Math.round(channel + (target - channel) * amount)
          .toString(16)
          .padStart(2, '0'),
      )
      .join('')}`
  const shades = {
    50: mix(0.94, 255),
    100: mix(0.86, 255),
    200: mix(0.72, 255),
    300: mix(0.52, 255),
    400: mix(0.28, 255),
    500: color,
    600: mix(0.18, 0),
    700: mix(0.35, 0),
    800: mix(0.52, 0),
    900: mix(0.68, 0),
  }
  function contrast(hex: string) {
    const linear = [1, 3, 5].map((offset) => {
      const channel = Number.parseInt(hex.slice(offset, offset + 2), 16) / 255
      return channel <= 0.04045 ? channel / 12.92 : ((channel + 0.055) / 1.055) ** 2.4
    })
    const luminance = linear[0]! * 0.2126 + linear[1]! * 0.7152 + linear[2]! * 0.0722
    return luminance > 0.179 ? '#000000' : '#ffffff'
  }
  root.dataset.brand = color
  for (const [stop, shade] of Object.entries(shades)) {
    root.style.setProperty(`--lg-brand-${stop}`, shade)
  }
  root.style.setProperty('--lg-brand-contrast', contrast(color))
  root.style.setProperty('--lg-brand-hover-contrast', contrast(shades[600]))
}
