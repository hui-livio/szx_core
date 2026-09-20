<script setup lang="ts">
import { useSessionNavigation } from './composables/useAuthViewModel'
import AdminShell from './components/AdminShell.vue'
const { authenticated } = useSessionNavigation()
</script>

<template>
  <AdminShell v-if="authenticated"><RouterView /></AdminShell><RouterView v-else />
</template>
