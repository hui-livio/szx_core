<script setup lang="ts">
import { computed, ref } from 'vue'
import { useI18n } from 'vue-i18n'
import {
  LuminaAppearanceControls,
  LuminaButton,
  LuminaDialog,
  LuminaInput,
  LUMINA_DEFAULT_BRAND,
} from '../ui/lumina'
import { useUiPreferences } from '../composables/useUiPreferences'
import LanguageSwitcher from './LanguageSwitcher.vue'

const { t } = useI18n()
const { theme, effects, brand, setTheme, setEffects, setBrand } = useUiPreferences()
const colorDialog = ref(false)
const selectedColor = computed({
  get: () => brand.value ?? LUMINA_DEFAULT_BRAND,
  set: (value: string | null | undefined) => {
    if (value) setBrand(value)
  },
})
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
    <LuminaButton
      class="lumina-preference-button"
      :aria-label="t('appearance.customize')"
      :title="t('appearance.customize')"
      aria-haspopup="dialog"
      @click="colorDialog = true"
    >
      <svg
        viewBox="0 0 24 24"
        fill="none"
        stroke="currentColor"
        stroke-width="2"
        shape-rendering="geometricPrecision"
        aria-hidden="true"
      >
        <path
          d="M12 3a9 9 0 1 0 0 18h1a2 2 0 0 0 1-3.7 1.5 1.5 0 0 1 .8-2.8H17A4 4 0 0 0 21 11a9 9 0 0 0-9-8Z"
        />
        <circle cx="7.5" cy="10" r=".7" />
        <circle cx="11" cy="7" r=".7" />
        <circle cx="15.5" cy="8" r=".7" />
      </svg>
    </LuminaButton>
    <LuminaAppearanceControls
      :theme="theme"
      :effects="effects"
      :labels="labels"
      @update:theme="setTheme"
      @update:effects="setEffects"
    />
    <LuminaDialog
      v-if="colorDialog"
      :title="t('appearance.customize')"
      :close-label="t('common.close')"
      @close="colorDialog = false"
    >
      <label class="brand-color-field">
        {{ t('appearance.brand') }}
        <LuminaInput v-model="selectedColor" type="color" />
      </label>
      <template #footer>
        <LuminaButton :disabled="brand === null" @click="setBrand(null)">
          {{ t('appearance.resetBrand') }}
        </LuminaButton>
      </template>
    </LuminaDialog>
  </div>
</template>
<style scoped>
.brand-color-field {
  display: flex;
  align-items: center;
  gap: var(--lg-s7);
}
.brand-color-field input {
  width: calc(var(--k) * 72px);
  height: var(--lg-h-ctrl);
  padding: var(--lg-s2);
  cursor: pointer;
}
</style>
