<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, watch } from 'vue'
import { balancedCardRows } from '../utils/cardGrid'
import { toCssLength } from '../utils/cssLength'

const props = withDefaults(
  defineProps<{ count: number; minCardWidth?: number; gap?: number | string }>(),
  { minCardWidth: 320, gap: 'var(--lg-s7)' },
)
const host = ref<HTMLElement>()
const width = ref(0)
const measuredGap = ref(16)
let resize: ResizeObserver | undefined
let measureFrame: number | undefined
const minimum = computed(() =>
  Number.isFinite(props.minCardWidth) && props.minCardWidth > 0 ? props.minCardWidth : 320,
)
const rows = computed(() =>
  balancedCardRows(
    props.count,
    Math.floor((width.value + measuredGap.value) / (minimum.value + measuredGap.value)),
  ),
)
const rowSizes = computed(() => rows.value.flatMap((size) => Array<number>(size).fill(size)))
function cellStyle(index: number) {
  const size = rowSizes.value[index] ?? 1
  return {
    flex: `1 0 calc((100% - var(--lumina-card-grid-gap) * ${size - 1}) / ${size})`,
    width: 'auto',
  }
}
function measureGap() {
  if (host.value) measuredGap.value = Number.parseFloat(getComputedStyle(host.value).columnGap) || 0
}
watch(() => props.gap, measureGap, { flush: 'post' })
onMounted(() => {
  if (!host.value) return
  resize = new ResizeObserver((entries) => {
    const entry = entries[0]
    if (!entry) return
    if (measureFrame !== undefined) cancelAnimationFrame(measureFrame)
    // Publish layout changes in the next frame, not inside the observer delivery loop.
    measureFrame = requestAnimationFrame(() => {
      measureFrame = undefined
      width.value = entry.contentRect.width
      measureGap()
    })
  })
  resize.observe(host.value)
  width.value = host.value.clientWidth
  measureGap()
})
onBeforeUnmount(() => {
  resize?.disconnect()
  if (measureFrame !== undefined) cancelAnimationFrame(measureFrame)
})
</script>
<template>
  <section
    ref="host"
    class="lumina-card-grid"
    :style="{ '--lumina-card-grid-gap': toCssLength(gap) }"
  >
    <slot :cell-style="cellStyle" :rows="rows" />
  </section>
</template>
