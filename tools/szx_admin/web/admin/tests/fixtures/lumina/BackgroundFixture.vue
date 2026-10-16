<script setup lang="ts">
import { ref } from 'vue'
import { LuminaBackground, useLuminaScale } from '../../../src/ui/lumina'

useLuminaScale()
const mounted = ref(true)
function toggleEffects() {
  const root = document.documentElement
  root.dataset.fx = root.dataset.fx === 'full' ? 'lite' : 'full'
}
</script>
<template>
  <LuminaBackground v-if="mounted" />
  <main style="position: relative; z-index: 1; padding: 24px">
    <button type="button" @click="toggleEffects">Toggle effects</button>
    <button type="button" @click="mounted = !mounted">Toggle background</button>
  </main>
</template>
