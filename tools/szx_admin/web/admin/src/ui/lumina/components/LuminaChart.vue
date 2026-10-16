<script setup lang="ts">
import { onBeforeUnmount, onMounted, ref, watch } from 'vue'
import { init, use, type EChartsType } from 'echarts/core'
import { LineChart, PieChart } from 'echarts/charts'
import { GridComponent, TooltipComponent } from 'echarts/components'
import { SVGRenderer } from 'echarts/renderers'
import type { LuminaChartOption } from '../types/chart'
import { createUpdateSnapshot } from '../utils/updateSnapshot'

use([LineChart, PieChart, GridComponent, TooltipComponent, SVGRenderer])
const props = withDefaults(
  defineProps<{ option: LuminaChartOption; label: string; animation?: boolean }>(),
  { animation: true },
)
const host = ref<HTMLDivElement>()
let chart: EChartsType | undefined
let resize: ResizeObserver | undefined
let resizeFrame: number | undefined
let appearance: MutationObserver | undefined
let motion: MediaQueryList | undefined
const rendered = createUpdateSnapshot()

function update() {
  if (!host.value || !chart) return
  const css = getComputedStyle(host.value)
  const text = css.getPropertyValue('--text').trim() || '#2d3656'
  const muted = css.getPropertyValue('--muted').trim() || '#7e88a8'
  const surface = css.getPropertyValue('--surface').trim() || '#fff'
  const full = document.documentElement.dataset.fx === 'full'
  const animation = props.animation && full && !motion?.matches
  const input = [props.option, text, muted, surface, css.fontFamily, animation]
  if (rendered.matches(input)) return
  chart.setOption(
    {
      color: ['#8870ee', '#47b8aa', '#edb768'],
      textStyle: { color: text, fontFamily: css.fontFamily },
      ...props.option,
      tooltip: {
        ...props.option.tooltip,
        backgroundColor: surface,
        borderColor: muted,
        textStyle: { color: text },
        // Keep tooltips inside the card's clipping/rounded boundary. No document-level portal.
        confine: true,
        renderMode: 'richText',
      },
      animation,
      animationDuration: animation ? 250 : 0,
      animationDurationUpdate: animation ? 250 : 0,
    },
    { notMerge: true, lazyUpdate: true },
  )
  // Commit only after setOption succeeds, and never hand its mutable option
  // object back to callers or use chart-owned data as a comparison baseline.
  rendered.commit(input)
}
onMounted(() => {
  if (!host.value) return
  chart = init(host.value, undefined, { renderer: 'svg' })
  motion = window.matchMedia('(prefers-reduced-motion: reduce)')
  motion.addEventListener('change', update)
  resize = new ResizeObserver(() => {
    if (resizeFrame !== undefined) return
    resizeFrame = requestAnimationFrame(() => {
      resizeFrame = undefined
      chart?.resize()
    })
  })
  resize.observe(host.value)
  appearance = new MutationObserver(update)
  appearance.observe(document.documentElement, {
    attributes: true,
    attributeFilter: ['data-theme', 'data-fx'],
  })
  update()
})
watch(() => [props.option, props.animation], update, { deep: true })
onBeforeUnmount(() => {
  resize?.disconnect()
  if (resizeFrame !== undefined) cancelAnimationFrame(resizeFrame)
  appearance?.disconnect()
  motion?.removeEventListener('change', update)
  chart?.dispose()
  chart = undefined
})
</script>
<template>
  <div ref="host" class="lumina-chart" role="img" :aria-label="label" />
</template>
