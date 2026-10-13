export type LuminaTheme = 'light' | 'dark'
export type LuminaEffects = 'lite' | 'full'

export function applyLuminaAppearance(
  appearance: { theme?: LuminaTheme; effects?: LuminaEffects },
  target?: HTMLElement,
) {
  const root = target ?? (typeof document === 'undefined' ? undefined : document.documentElement)
  if (!root) return
  if (appearance.theme) root.dataset.theme = appearance.theme
  if (appearance.effects) root.dataset.fx = appearance.effects
}
