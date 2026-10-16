import { shallowRef } from 'vue'
import { acceptHMRUpdate, defineStore } from 'pinia'
import {
  applyLuminaAppearance,
  applyLuminaBrand,
  normalizeLuminaBrand,
  type LuminaTheme,
  type LuminaEffects,
} from '../ui/lumina/theme/appearance'

const themeKey = 'szx-admin-theme'
const effectsKey = 'szx-admin-effects'
const brandKey = 'szx-admin-brand'
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
  const brand = shallowRef<string | null>(null)
  const initialized = shallowRef(false)
  const apply = () => applyLuminaAppearance({ theme: theme.value, effects: effects.value })

  function initialize() {
    if (!initialized.value) {
      theme.value = readPreference(themeKey) === 'dark' ? 'dark' : 'light'
      effects.value = readPreference(effectsKey) === 'full' ? 'full' : 'lite'
      brand.value = normalizeLuminaBrand(readPreference(brandKey) ?? '')
      initialized.value = true
    }
    apply()
    applyLuminaBrand(brand.value)
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
  function setBrand(value: string | null) {
    const normalized = value === null ? null : normalizeLuminaBrand(value)
    if (value !== null && normalized === null) return
    brand.value = normalized
    applyLuminaBrand(normalized)
    savePreference(brandKey, normalized ?? '')
  }
  return { theme, effects, brand, initialized, initialize, setTheme, setEffects, setBrand }
})

if (import.meta.hot) import.meta.hot.accept(acceptHMRUpdate(useUiPreferencesStore, import.meta.hot))
