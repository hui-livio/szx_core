<script setup lang="ts">
import { onBeforeUnmount, onMounted, watchEffect } from 'vue'
import { usePrimeVue } from 'primevue/config'
import { useI18n } from 'vue-i18n'
import { useSessionNavigation } from './composables/useAuthViewModel'
import { datePickerLocale } from './i18n/primevue'
import type { SupportedLocale } from './i18n'
import AdminShell from './components/AdminShell.vue'
const { authenticated } = useSessionNavigation()
const { t, locale } = useI18n()
const primevue = usePrimeVue()
watchEffect(() => {
  if (primevue.config.locale)
    Object.assign(primevue.config.locale, datePickerLocale(locale.value as SupportedLocale))
  const aria = primevue.config.locale?.aria
  if (!aria) return
  Object.assign(aria, {
    prevPageLabel: t('common.previous'),
    nextPageLabel: t('common.next'),
    close: t('common.close'),
  })
})

// Keep the theme's continuous scaling, without shrinking long admin pages to fit a card.
function updateThemeScale() {
  const base = Math.min(window.innerWidth / 1440, window.innerHeight / 900)
  const scale = Math.max(0.92, Math.min(1.5, 1 + (base - 1) * 0.7))
  document.documentElement.style.setProperty('--k', scale.toFixed(2))
}

function preventNativeContextMenu(event: MouseEvent) {
  event.preventDefault()
}

onMounted(() => {
  updateThemeScale()
  window.addEventListener('resize', updateThemeScale)
  // Document scope also covers PrimeVue overlays teleported outside #app.
  document.addEventListener('contextmenu', preventNativeContextMenu)
})
onBeforeUnmount(() => {
  window.removeEventListener('resize', updateThemeScale)
  document.removeEventListener('contextmenu', preventNativeContextMenu)
})
</script>

<template>
  <div class="lg-bg" aria-hidden="true">
    <div class="lg-orb a" />
    <div class="lg-orb b" />
    <div class="lg-orb c" />
    <div class="lg-grid" />
  </div>
  <RouterView v-slot="{ Component, route }">
    <AdminShell v-if="authenticated">
      <component :is="Component" v-if="route.path !== '/login'" />
      <p v-else class="hint" role="status">{{ t('common.loading') }}</p>
    </AdminShell>
    <component :is="Component" v-else-if="route.path === '/login'" />
  </RouterView>
</template>
