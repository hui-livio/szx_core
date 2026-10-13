<script setup lang="ts">
import { useI18n } from 'vue-i18n'
import { LuminaDateTimePicker } from '../ui/lumina'
const model = defineModel<string>({ required: true })
defineProps<{ label: string }>()
const { t } = useI18n()
</script>
<template>
  <LuminaDateTimePicker v-model="model" :label="label" :invalid-text="t('datePicker.invalid')" />
</template>
