<script setup lang="ts">
import { ref } from 'vue'
import { useLuminaBackground } from '../composables/useLuminaBackground'
import { LUMINA_BACKGROUND_SPARKS } from '../utils/background'

const host = ref<HTMLElement>()
useLuminaBackground(host)
const particles = Array.from({ length: LUMINA_BACKGROUND_SPARKS }, (_, index) => ({
  left: `${((index * 61.8034) % 100).toFixed(2)}%`,
}))
</script>

<template>
  <div ref="host" class="lg-bg" data-motion="paused" aria-hidden="true">
    <div class="lg-orb a" />
    <div class="lg-orb b" />
    <div class="lg-orb c" />
    <div class="lg-spot" />
    <div v-for="(style, index) in particles" :key="index" class="lg-spark" :style="style" />
  </div>
</template>
