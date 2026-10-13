<script setup lang="ts">
import { useI18n } from 'vue-i18n'
import { LuminaDialog } from '../ui/lumina'
defineProps<{ title: string; busy?: boolean }>()
const emit = defineEmits<{ close: [] }>()
const { t } = useI18n()
</script>
<template>
  <LuminaDialog
    class="modal"
    :title="title"
    :busy="busy"
    :close-label="t('common.close')"
    @close="emit('close')"
  >
    <slot />
  </LuminaDialog>
</template>
