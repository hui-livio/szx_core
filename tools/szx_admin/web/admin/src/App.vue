<script setup lang="ts">
import { onBeforeUnmount, onMounted, watchEffect } from 'vue'
import { LuminaBackground, useLuminaLocale, useLuminaScale } from './ui/lumina'
import { useI18n } from 'vue-i18n'
import { useSessionNavigation } from './composables/useAuthViewModel'
import { datePickerLocale } from './i18n/primevue'
import type { SupportedLocale } from './i18n'
import AdminShell from './components/AdminShell.vue'
const { authenticated } = useSessionNavigation()
const { t, locale } = useI18n()
const updateLocale = useLuminaLocale()
useLuminaScale()
watchEffect(() => {
  updateLocale({
    ...datePickerLocale(locale.value as SupportedLocale),
    aria: {
      prevPageLabel: t('common.previous'),
      nextPageLabel: t('common.next'),
      close: t('common.close'),
    },
  })
})

function preventNativeContextMenu(event: MouseEvent) {
  event.preventDefault()
}

onMounted(() => {
  // Document scope also covers PrimeVue overlays teleported outside #app.
  document.addEventListener('contextmenu', preventNativeContextMenu)
})
onBeforeUnmount(() => {
  document.removeEventListener('contextmenu', preventNativeContextMenu)
})
</script>

<template>
  <LuminaBackground />
  <RouterView v-slot="{ Component, route }">
    <AdminShell v-if="authenticated">
      <component :is="Component" v-if="route.path !== '/login'" />
      <p v-else class="hint" role="status">{{ t('common.loading') }}</p>
    </AdminShell>
    <component :is="Component" v-else-if="route.path === '/login'" />
  </RouterView>
</template>
