import { computed, onScopeDispose, shallowRef } from 'vue'
import { acceptHMRUpdate, defineStore } from 'pinia'
import {
  applyLuminaAppearance,
  applyLuminaBrand,
  normalizeLuminaBrand,
  onSystemThemeChange,
  systemPrefersDark,
  type LuminaTheme,
  type LuminaThemePreference,
  type LuminaEffects,
} from '../ui/lumina/theme/appearance'

const themeKey = 'szx-admin-theme'
const effectsKey = 'szx-admin-effects'
const brandKey = 'szx-admin-brand'
const designKey = 'szx-admin-design'
export type UiDesign = 'glass' | 'industrial'
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
  const themePreference = shallowRef<LuminaThemePreference>('light')
  const systemDark = shallowRef(false)
  const theme = computed<LuminaTheme>(() =>
    themePreference.value === 'system'
      ? systemDark.value
        ? 'dark'
        : 'light'
      : themePreference.value,
  )
  const effects = shallowRef<LuminaEffects>('lite')
  const design = shallowRef<UiDesign>('glass')
  const brand = shallowRef<string | null>(null)
  const initialized = shallowRef(false)
  let stopSystemTheme: (() => void) | undefined
  const apply = () => {
    if (typeof document !== 'undefined') document.documentElement.dataset.design = design.value
    applyLuminaAppearance({
      theme: theme.value,
      effects: design.value === 'industrial' ? 'lite' : effects.value,
    })
    applyLuminaBrand(design.value === 'industrial' ? null : brand.value)
  }

  function initialize() {
    if (!initialized.value) {
      const savedTheme = readPreference(themeKey)
      themePreference.value =
        savedTheme === 'dark' || savedTheme === 'system' ? savedTheme : 'light'
      systemDark.value = systemPrefersDark()
      effects.value = readPreference(effectsKey) === 'full' ? 'full' : 'lite'
      design.value = readPreference(designKey) === 'industrial' ? 'industrial' : 'glass'
      brand.value = normalizeLuminaBrand(readPreference(brandKey) ?? '')
      initialized.value = true
      stopSystemTheme = onSystemThemeChange((dark) => {
        systemDark.value = dark
        if (themePreference.value === 'system') apply()
      })
    }
    apply()
  }
  function setTheme(value: LuminaTheme) {
    setThemePreference(value)
  }
  function setThemePreference(value: LuminaThemePreference) {
    themePreference.value = value
    apply()
    savePreference(themeKey, value)
  }
  function setEffects(value: LuminaEffects) {
    effects.value = value
    apply()
    savePreference(effectsKey, value)
  }
  function setDesign(value: UiDesign) {
    design.value = value
    apply()
    savePreference(designKey, value)
  }
  function setBrand(value: string | null) {
    const normalized = value === null ? null : normalizeLuminaBrand(value)
    if (value !== null && normalized === null) return
    brand.value = normalized
    applyLuminaBrand(design.value === 'industrial' ? null : normalized)
    savePreference(brandKey, normalized ?? '')
  }
  onScopeDispose(() => stopSystemTheme?.())
  return {
    theme,
    themePreference,
    effects,
    design,
    brand,
    initialized,
    initialize,
    setTheme,
    setThemePreference,
    setEffects,
    setDesign,
    setBrand,
  }
})

if (import.meta.hot) import.meta.hot.accept(acceptHMRUpdate(useUiPreferencesStore, import.meta.hot))
