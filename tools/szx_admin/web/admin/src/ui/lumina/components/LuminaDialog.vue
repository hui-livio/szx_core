<script setup lang="ts">
import Dialog from 'primevue/dialog'
import { computed, nextTick, onBeforeUnmount, onUnmounted, useAttrs, useId } from 'vue'
import LuminaButton from './LuminaButton.vue'
import LuminaPanel from './LuminaPanel.vue'
import { registerLuminaDialog, topLuminaDialog } from '../utils/dialogStack'

defineOptions({ inheritAttrs: false })
const props = defineProps<{ title: string; busy?: boolean; closeLabel: string }>()
const emit = defineEmits<{ close: [] }>()
const attrs = useAttrs()
const fallbackId = useId()
const dialogId = computed(() => String(attrs.id ?? fallbackId))
const titleId = computed(() => `${dialogId.value}-title`)
const element = () => document.getElementById(dialogId.value)
const opener =
  typeof document !== 'undefined' && document.activeElement instanceof HTMLElement
    ? document.activeElement
    : null

function requestClose() {
  // This guard is evaluated at the time of every request, not just on mount.
  // Business owners may still veto close (e.g. unsaved-change confirmation).
  if (!props.busy) emit('close')
}

// Register in setup order so a nested dialog always sits above its parent.
const unregister =
  typeof document === 'undefined' ? undefined : registerLuminaDialog({ element, requestClose })
onBeforeUnmount(() => unregister?.())
onUnmounted(() => {
  void nextTick(() => {
    const top = topLuminaDialog()
    if (opener?.isConnected && (!top || top.contains(opener))) opener.focus()
  })
})

function focusDialog() {
  void nextTick(() => {
    const root = element()
    if (!root || root !== topLuminaDialog() || root.contains(document.activeElement)) return
    const initial = root.querySelector<HTMLElement>('[autofocus]:not(:disabled)') ?? root
    initial.focus()
  })
}
</script>
<template>
  <Dialog
    v-bind="$attrs"
    :id="dialogId"
    :visible="true"
    modal
    class="lumina-dialog"
    :header="title"
    :aria-labelledby="titleId"
    :aria-busy="Boolean(busy)"
    tabindex="-1"
    :close-on-escape="false"
    :dismissable-mask="false"
    :draggable="false"
    @show="focusDialog"
    @update:visible="!$event && requestClose()"
  >
    <template #container>
      <LuminaPanel class="lumina-dialog-panel" variant="solid" :padding="0">
        <header class="p-dialog-header">
          <span :id="titleId" class="p-dialog-title"
            ><slot name="header">{{ title }}</slot></span
          >
          <LuminaButton
            class="p-dialog-close-button"
            :aria-label="closeLabel"
            :disabled="busy"
            autofocus
            @click="requestClose"
          >
            <svg
              viewBox="0 0 24 24"
              width="16"
              height="16"
              fill="none"
              stroke="currentColor"
              stroke-width="2"
              aria-hidden="true"
            >
              <path d="m6 6 12 12M18 6 6 18" />
            </svg>
          </LuminaButton>
        </header>
        <div class="p-dialog-content"><slot /></div>
        <footer v-if="$slots.footer" class="p-dialog-footer"><slot name="footer" /></footer>
      </LuminaPanel>
    </template>
  </Dialog>
</template>
