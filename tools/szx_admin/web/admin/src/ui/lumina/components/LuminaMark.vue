<script setup lang="ts">
import { computed } from 'vue'
import { toCssLength } from '../utils/cssLength'

const props = withDefaults(
  defineProps<{
    text?: string
    label?: string
    variant?: 'brand' | 'soft'
    size?: number | string
    radius?: number | string
    fontSize?: number | string
    fontWeight?: number | string
  }>(),
  {
    text: 'S',
    variant: 'brand',
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
  <span
    class="lumina-mark"
    :class="`lumina-mark-${variant}`"
    :style="style"
    :role="label ? 'img' : undefined"
    :aria-label="label"
    :aria-hidden="label ? undefined : true"
    >{{ text }}</span
  >
</template>
