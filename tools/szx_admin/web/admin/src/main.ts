import { createApp } from 'vue'
import { createPinia } from 'pinia'
import PrimeVue from 'primevue/config'
import { LuminaAura } from './theme/luminaAura'
import './style.css'
import App from './App.vue'
import { router } from './router'
import { i18n } from './i18n'

createApp(App)
  .use(createPinia())
  .use(PrimeVue, {
    ripple: false,
    theme: {
      preset: LuminaAura,
      options: {
        darkModeSelector: '[data-theme="dark"]',
        cssLayer: { name: 'primevue', order: 'lumina, primevue, app' },
      },
    },
  })
  .use(router)
  .use(i18n)
  .mount('#app')
