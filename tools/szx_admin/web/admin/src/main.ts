import { createApp } from 'vue'
import { createPinia } from 'pinia'
import { createLuminaUi } from './ui/lumina'
import './style.css'
import App from './App.vue'
import { router } from './router'
import { i18n } from './i18n'
import { initializeUiPreferences } from './composables/useUiPreferences'

initializeUiPreferences()

createApp(App).use(createPinia()).use(createLuminaUi()).use(router).use(i18n).mount('#app')
