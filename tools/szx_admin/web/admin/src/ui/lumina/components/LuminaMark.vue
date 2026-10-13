<script setup lang="ts">
import { computed } from 'vue'

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
const length = (value: number | string) =>
  typeof value === 'number' ? `${Math.max(0, value)}px` : value
const style = computed(() => ({
  '--lumina-mark-size': length(props.size),
  '--lumina-mark-radius': length(props.radius),
  '--lumina-mark-font-size': length(
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
