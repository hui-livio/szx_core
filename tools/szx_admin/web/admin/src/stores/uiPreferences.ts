import { shallowRef } from 'vue'
import { acceptHMRUpdate, defineStore } from 'pinia'
import {
  applyLuminaAppearance,
  type LuminaTheme,
  type LuminaEffects,
} from '../ui/lumina/theme/appearance'

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
  try {
    if (typeof localStorage !== 'undefined') localStorage.setItem(key, value)
  } catch {
    /* Visual preferences remain usable when browser storage is blocked. */
  }
}

export const useUiPreferencesStore = defineStore('ui-preferences', () => {
  const theme = shallowRef<LuminaTheme>('light')
  const effects = shallowRef<LuminaEffects>('lite')
  const initialized = shallowRef(false)
  const apply = () => applyLuminaAppearance({ theme: theme.value, effects: effects.value })

  function initialize() {
    if (!initialized.value) {
      theme.value = readPreference(themeKey) === 'dark' ? 'dark' : 'light'
      effects.value = readPreference(effectsKey) === 'full' ? 'full' : 'lite'
      initialized.value = true
    }
    apply()
  }
  function setTheme(value: LuminaTheme) {
    theme.value = value
    apply()
    savePreference(themeKey, value)
  }
  function setEffects(value: LuminaEffects) {
    effects.value = value
    apply()
    savePreference(effectsKey, value)
  }
  return { theme, effects, initialized, initialize, setTheme, setEffects }
})

if (import.meta.hot) import.meta.hot.accept(acceptHMRUpdate(useUiPreferencesStore, import.meta.hot))
