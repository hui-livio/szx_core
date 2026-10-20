<script setup lang="ts">
import LuminaButton from './LuminaButton.vue'
import type { LuminaTheme, LuminaThemePreference, LuminaEffects } from '../theme/appearance'

const theme = defineModel<LuminaThemePreference>('theme', { required: true })
const effects = defineModel<LuminaEffects>('effects', { required: true })
const props = defineProps<{
  resolvedTheme: LuminaTheme
  showEffects?: boolean
  labels: {
    theme: string
    performance: string
    system: string
    light: string
    dark: string
    lite: string
    full: string
  }
}>()
const themeCycle: LuminaThemePreference[] = ['light', 'dark', 'system']
function cycleTheme() {
  theme.value = themeCycle[(themeCycle.indexOf(theme.value) + 1) % themeCycle.length]!
}
</script>
<template>
  <div class="lumina-appearance-controls">
    <LuminaButton
      v-if="props.showEffects !== false"
      class="lumina-preference-button"
      role="switch"
      :aria-label="labels.performance"
      :aria-checked="effects === 'lite'"
      :title="effects === 'lite' ? labels.lite : labels.full"
      @click="effects = effects === 'lite' ? 'full' : 'lite'"
    >
      <svg
        viewBox="0 0 24 24"
        fill="none"
        stroke="currentColor"
        stroke-width="2"
        shape-rendering="geometricPrecision"
        aria-hidden="true"
      >
        <path d="m13 2-9 12h7l-1 8 10-13h-7l1-7Z" stroke-linejoin="round" />
      </svg>
    </LuminaButton>
    <LuminaButton
      class="lumina-preference-button"
      role="switch"
      :aria-label="labels.theme"
      :aria-checked="resolvedTheme === 'dark'"
      :title="`${labels.theme}: ${theme === 'system' ? labels.system : theme === 'dark' ? labels.dark : labels.light}`"
      @click="cycleTheme"
    >
      <svg
        v-if="theme === 'system'"
        viewBox="0 0 24 24"
        fill="none"
        stroke="currentColor"
        stroke-width="2"
        shape-rendering="geometricPrecision"
        aria-hidden="true"
      >
        <circle cx="12" cy="12" r="9" />
        <path d="M12 3v18a9 9 0 0 0 0-18Z" fill="currentColor" stroke="none" />
      </svg>
      <svg
        v-else-if="theme === 'light'"
        viewBox="0 0 24 24"
        fill="none"
        stroke="currentColor"
        stroke-width="2"
        shape-rendering="geometricPrecision"
        aria-hidden="true"
      >
        <path
          d="M20.5 14.2A8.5 8.5 0 0 1 9.8 3.5a8.5 8.5 0 1 0 10.7 10.7Z"
          stroke-linejoin="round"
        />
      </svg>
      <svg
        v-else
        viewBox="0 0 24 24"
        fill="none"
        stroke="currentColor"
        stroke-width="2"
        shape-rendering="geometricPrecision"
        aria-hidden="true"
      >
        <circle cx="12" cy="12" r="4" />
        <path
          d="M12 2v2m0 16v2M2 12h2m16 0h2M5 5l1.5 1.5m11 11L19 19M5 19l1.5-1.5m11-11L19 5"
          stroke-linecap="round"
        />
      </svg>
    </LuminaButton>
  </div>
</template>
