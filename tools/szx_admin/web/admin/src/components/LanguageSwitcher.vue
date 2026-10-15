<script setup lang="ts">
import { computed } from 'vue'
import { LuminaSelect as Select } from '../ui/lumina'
import { useI18n } from 'vue-i18n'
import { localeOptions, setLocale } from '../i18n'

const { locale, t } = useI18n()
const options = [...localeOptions]
const selectedLabel = computed(() => options.find((option) => option.value === locale.value)?.label)
function selectLocale(value: string) {
  const option = localeOptions.find((item) => item.value === value)
  if (option) setLocale(option.value)
}
</script>

<template>
  <div class="language-switcher lumina-smooth-edge">
    <Select
      :model-value="locale"
      :options="options"
      option-label="label"
      option-value="value"
      :aria-label="`${t('language.switch')}: ${selectedLabel}`"
      :title="`${t('language.switch')}: ${selectedLabel}`"
      @update:model-value="selectLocale"
    >
      <template #value>
        <span class="language-value">{{ selectedLabel }}</span>
        <svg
          class="language-button-icon"
          viewBox="0 0 24 24"
          fill="none"
          stroke="currentColor"
          stroke-width="1.8"
          stroke-linecap="round"
          stroke-linejoin="round"
          aria-hidden="true"
        >
          <path d="M3 5h12M9 3v2M5 5c0 6 4 9 8 11M13 5c0 6-4 9-10 12m11 3 4-10 4 10m-6.8-3h5.6" />
        </svg>
      </template>
    </Select>
  </div>
</template>
