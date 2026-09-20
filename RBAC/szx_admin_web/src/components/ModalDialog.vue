<script setup lang="ts">
import { useId } from 'vue'
import { useDialog } from '../composables/useDialog'
defineProps<{ title: string; busy?: boolean }>()
const emit = defineEmits<{ close: [] }>()
const titleId = useId()
useDialog()
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
        aria-label="关闭对话框"
        :disabled="busy"
        @click="emit('close')"
      >
        ×
      </button>
    </header>
    <slot />
  </dialog>
</template>
