<script setup lang="ts">
import { useI18n } from 'vue-i18n'
import { localeOptions, setLocale, type SupportedLocale } from '../i18n'

const { locale, t } = useI18n()

function selectLocale(event: Event) {
  const value = (event.target as HTMLSelectElement).value as SupportedLocale
  setLocale(value)
}
</script>

<template>
  <div class="language-switcher">
    <span class="language-icon" aria-hidden="true">文A</span>
    <select :value="locale" :aria-label="t('language.switch')" @change="selectLocale">
      <option v-for="item in localeOptions" :key="item.value" :value="item.value">
        {{ item.label }}
      </option>
    </select>
  </div>
</template>
