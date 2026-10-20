import { readonly } from 'vue'
import { storeToRefs, type Pinia } from 'pinia'
import { useUiPreferencesStore } from '../stores/uiPreferences'

export function initializeUiPreferences(pinia: Pinia) {
  useUiPreferencesStore(pinia).initialize()
}

export function useUiPreferences() {
  const store = useUiPreferencesStore()
  const { theme, themePreference, effects, design, brand } = storeToRefs(store)
  return {
    theme: readonly(theme),
    themePreference: readonly(themePreference),
    effects: readonly(effects),
    design: readonly(design),
    brand: readonly(brand),
    setBrand: store.setBrand,
    setTheme: store.setTheme,
    setThemePreference: store.setThemePreference,
    setEffects: store.setEffects,
    setDesign: store.setDesign,
  }
}
