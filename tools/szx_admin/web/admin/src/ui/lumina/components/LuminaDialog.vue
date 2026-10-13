<script setup lang="ts">
import Dialog from 'primevue/dialog'
import { nextTick, onUnmounted } from 'vue'
const props = defineProps<{ title: string; busy?: boolean; closeLabel: string }>()
const emit = defineEmits<{ close: [] }>()
// Parent views also unmount the dialog after successful writes or explicit cancellation.
const opener =
  typeof document !== 'undefined' && document.activeElement instanceof HTMLElement
    ? document.activeElement
    : null
onUnmounted(() => {
  void nextTick(() => {
    if (opener?.isConnected && !document.querySelector('[role="dialog"]')) opener.focus()
  })
})

function closeFromSelect(event: KeyboardEvent) {
  if (event.key !== 'Escape' || event.isComposing || props.busy) return
  const control = event.target instanceof Element ? event.target.closest('[role="combobox"]') : null
  // PrimeVue Select consumes Escape even when its list is already closed.
  if (control?.getAttribute('aria-expanded') === 'false') {
    event.preventDefault()
    event.stopPropagation()
    emit('close')
  }
}
</script>
<template>
  <Dialog
    :visible="true"
    modal
    class="lumina-dialog"
    :header="title"
    :close-on-escape="!busy"
    :dismissable-mask="false"
    :draggable="false"
    :close-button-props="{
      'aria-label': closeLabel,
      disabled: busy,
      severity: 'secondary',
      text: true,
    }"
    @keydown.capture="closeFromSelect"
    @update:visible="!$event && !busy && emit('close')"
  >
    <template v-if="$slots.header" #header><slot name="header" /></template>
    <slot />
    <template v-if="$slots.footer" #footer><slot name="footer" /></template>
  </Dialog>
</template>
