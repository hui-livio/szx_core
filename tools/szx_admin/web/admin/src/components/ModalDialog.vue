<script setup lang="ts">
import { useId } from 'vue'
import { useI18n } from 'vue-i18n'
import { useDialog } from '../composables/useDialog'
defineProps<{ title: string; busy?: boolean }>()
const emit = defineEmits<{ close: [] }>()
const titleId = useId()
useDialog()
const { t } = useI18n()
</script>
<template>
  <dialog
    ref="dialog"
    class="modal"
    :aria-labelledby="titleId"
    @cancel.prevent="!busy && emit('close')"
  >
    <header class="modal-header">
      <h2 :id="titleId">{{ title }}</h2>
      <button
        type="button"
        class="icon-button"
        :aria-label="t('common.close')"
        :disabled="busy"
        @click="emit('close')"
      >
        ×
      </button>
    </header>
    <slot />
  </dialog>
</template>
