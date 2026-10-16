<script setup lang="ts">
import { LuminaPanel } from '../ui/lumina'
import { useI18n } from 'vue-i18n'
const { t } = useI18n()
</script>
<template>
  <LuminaPanel as="section" class="panel empty" radius="var(--lg-r-3xl)" padding="var(--lg-s11)">
    <p class="eyebrow">404</p>
    <h1>{{ t('notFound.title') }}</h1>
    <p>{{ t('notFound.subtitle') }}</p>
    <RouterLink class="button primary" to="/home">{{ t('notFound.back') }}</RouterLink>
  </LuminaPanel>
</template>
