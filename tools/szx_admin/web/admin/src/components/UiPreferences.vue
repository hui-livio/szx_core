<script setup lang="ts">
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'
import { LuminaAppearanceControls } from '../ui/lumina'
import { useUiPreferences } from '../composables/useUiPreferences'
import LanguageSwitcher from './LanguageSwitcher.vue'

const { t } = useI18n()
const { theme, effects, setTheme, setEffects } = useUiPreferences()
const labels = computed(() => ({
  theme: t('appearance.theme'),
  performance: t('appearance.performance'),
  light: t('appearance.light'),
  dark: t('appearance.dark'),
  lite: t('appearance.lite'),
  full: t('appearance.full'),
}))
</script>
<template>
  <div class="ui-preferences">
    <LanguageSwitcher />
    <LuminaAppearanceControls
      :theme="theme"
      :effects="effects"
      :labels="labels"
      @update:theme="setTheme"
      @update:effects="setEffects"
    />
  </div>
</template>
