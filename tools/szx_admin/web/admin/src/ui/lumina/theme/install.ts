import type { Plugin } from 'vue'
import PrimeVue, { usePrimeVue } from 'primevue/config'
import type { PrimeVueLocaleOptions } from '@primevue/core/config'
import { LuminaAura } from './aura'

export type LuminaLocale = Partial<PrimeVueLocaleOptions>

export function createLuminaUi(options: { ripple?: boolean; locale?: LuminaLocale } = {}): Plugin {
  return {
    install(app) {
      // Root scope includes PrimeVue overlays teleported outside the Vue mount node.
      if (typeof document !== 'undefined') document.documentElement.dataset.luminaScrollbars = ''

      app.use(PrimeVue, {
        ripple: options.ripple ?? false,
        ...(options.locale ? { locale: options.locale } : {}),
        theme: {
          preset: LuminaAura,
          options: {
            darkModeSelector: '[data-theme="dark"]',
            cssLayer: { name: 'primevue', order: 'lumina, primevue, app' },
          },
        },
      })
    },
  }
}

export function useLuminaLocale() {
  const primevue = usePrimeVue()
  return (locale: LuminaLocale) => {
    const current = primevue.config.locale
    if (!current) return
    const { aria, ...rest } = locale
    Object.assign(current, rest)
    if (aria) Object.assign((current.aria ??= {}), aria)
  }
}
