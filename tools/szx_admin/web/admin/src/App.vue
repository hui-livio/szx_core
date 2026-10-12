<script setup lang="ts">
import { onBeforeUnmount, onMounted } from 'vue'
import { useSessionNavigation } from './composables/useAuthViewModel'
import AdminShell from './components/AdminShell.vue'
const { authenticated } = useSessionNavigation()

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
  <AdminShell v-if="authenticated">
    <RouterView />
  </AdminShell>
  <RouterView v-else />
</template>
