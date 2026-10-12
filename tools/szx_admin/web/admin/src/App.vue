<script setup lang="ts">
import { onBeforeUnmount, onMounted } from 'vue'
import { useI18n } from 'vue-i18n'
import { useSessionNavigation } from './composables/useAuthViewModel'
import AdminShell from './components/AdminShell.vue'
const { authenticated } = useSessionNavigation()
const { t } = useI18n()

// Keep the theme's continuous scaling, without shrinking long admin pages to fit a card.
function updateThemeScale() {
  const base = Math.min(window.innerWidth / 1440, window.innerHeight / 900)
  const scale = Math.max(0.92, Math.min(1.5, 1 + (base - 1) * 0.7))
  document.documentElement.style.setProperty('--k', scale.toFixed(2))
}

onMounted(() => {
  updateThemeScale()
  window.addEventListener('resize', updateThemeScale)
})
onBeforeUnmount(() => window.removeEventListener('resize', updateThemeScale))
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
