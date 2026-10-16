import { readonly } from 'vue'
import { storeToRefs, type Pinia } from 'pinia'
import { useUiPreferencesStore } from '../stores/uiPreferences'

export function initializeUiPreferences(pinia: Pinia) {
  useUiPreferencesStore(pinia).initialize()
}

export function useUiPreferences() {
  const store = useUiPreferencesStore()
  const { theme, effects, brand } = storeToRefs(store)
  return {
    theme: readonly(theme),
    effects: readonly(effects),
    brand: readonly(brand),
    setBrand: store.setBrand,
    setTheme: store.setTheme,
    setEffects: store.setEffects,
  }
}
