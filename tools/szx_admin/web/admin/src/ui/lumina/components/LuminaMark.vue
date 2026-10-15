<script setup lang="ts">
import { computed } from 'vue'
import LuminaButton from './LuminaButton.vue'
import { toCssLength } from '../utils/cssLength'

const props = withDefaults(
  defineProps<{
    text?: string
    label?: string
    variant?: 'brand' | 'soft'
    interactive?: boolean
    disabled?: boolean
    size?: number | string
    radius?: number | string
    fontSize?: number | string
    fontWeight?: number | string
  }>(),
  {
    text: 'S',
    variant: 'brand',
    interactive: false,
    disabled: false,
    size: 'var(--lg-h-ctrl)',
    radius: 'var(--lg-r-lg)',
    fontWeight: 500,
  },
)
const style = computed(() => ({
  '--lumina-mark-size': toCssLength(props.size),
  '--lumina-mark-radius': toCssLength(props.radius),
  '--lumina-mark-font-size': toCssLength(
    props.fontSize ?? (props.variant === 'brand' ? 'var(--lg-fs-display)' : 'var(--lg-fs-sm)'),
  ),
  '--lumina-mark-font-weight': String(props.fontWeight),
}))
</script>
<template>
  <component
    :is="interactive ? LuminaButton : 'span'"
    class="lumina-mark"
    :class="[`lumina-mark-${variant}`, { 'lumina-mark-button': interactive }]"
    :variant="interactive ? 'secondary' : undefined"
    :disabled="interactive ? disabled : undefined"
    :style="style"
    :role="!interactive && label ? 'img' : undefined"
    :aria-label="label"
    :aria-hidden="interactive || label ? undefined : true"
    >{{ text }}</component
  >
</template>
