<script setup lang="ts">
import Select from 'primevue/select'
import { useI18n } from 'vue-i18n'
import { localeOptions, setLocale } from '../i18n'

const { locale, t } = useI18n()
const options = [...localeOptions]
</script>

<template>
  <div class="language-switcher">
    <span class="language-icon" aria-hidden="true">文A</span>
    <Select
      :model-value="locale"
      :options="options"
      option-label="label"
      option-value="value"
      :aria-label="t('language.switch')"
      @update:model-value="setLocale"
    />
  </div>
</template>
