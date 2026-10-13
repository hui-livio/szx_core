<script setup lang="ts">
import { LuminaSelect as Select } from '../ui/lumina'
import { useI18n } from 'vue-i18n'
import { localeOptions, setLocale } from '../i18n'

const { locale, t } = useI18n()
const options = [...localeOptions]
function selectLocale(value: string) {
  const option = localeOptions.find((item) => item.value === value)
  if (option) setLocale(option.value)
}
</script>

<template>
  <div class="language-switcher lumina-smooth-edge">
    <span class="language-icon" aria-hidden="true">文A</span>
    <Select
      :model-value="locale"
      :options="options"
      option-label="label"
      option-value="value"
      :aria-label="t('language.switch')"
      @update:model-value="selectLocale"
    />
  </div>
</template>
