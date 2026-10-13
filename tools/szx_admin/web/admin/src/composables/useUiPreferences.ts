import { readonly, shallowRef } from 'vue'
import { applyLuminaAppearance, type LuminaTheme, type LuminaEffects } from '../ui/lumina'

const theme = shallowRef<LuminaTheme>('light')
const effects = shallowRef<LuminaEffects>('lite')
const themeKey = 'szx-admin-theme'
const effectsKey = 'szx-admin-effects'

function readPreference(key: string): string | null {
  try {
    return typeof localStorage === 'undefined' ? null : localStorage.getItem(key)
  } catch {
    return null
  }
}
function savePreference(key: string, value: string) {
  // Blocked/private browser storage must not prevent changing the current UI.
  try {
    if (typeof localStorage !== 'undefined') localStorage.setItem(key, value)
  } catch {
    /* Visual preferences are optional; keep the in-memory selection. */
  }
}

export function initializeUiPreferences() {
  theme.value = readPreference(themeKey) === 'dark' ? 'dark' : 'light'
  effects.value = readPreference(effectsKey) === 'full' ? 'full' : 'lite'
  applyLuminaAppearance({ theme: theme.value, effects: effects.value })
}

export function useUiPreferences() {
  return {
    theme: readonly(theme),
    effects: readonly(effects),
    setTheme(value: LuminaTheme) {
      theme.value = value
      applyLuminaAppearance({ theme: value })
      savePreference(themeKey, value)
    },
    setEffects(value: LuminaEffects) {
      effects.value = value
      applyLuminaAppearance({ effects: value })
      savePreference(effectsKey, value)
    },
  }
}
