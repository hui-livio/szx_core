<script setup lang="ts">
import { useI18n } from 'vue-i18n'
const { t } = useI18n()
</script>
<template>
  <section class="panel empty">
    <p class="eyebrow">404</p>
    <h1>{{ t('notFound.title') }}</h1>
    <p>{{ t('notFound.subtitle') }}</p>
    <RouterLink class="button primary" to="/home">{{ t('notFound.back') }}</RouterLink>
  </section>
</template>
