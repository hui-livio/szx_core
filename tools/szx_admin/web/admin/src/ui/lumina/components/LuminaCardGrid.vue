<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, watch } from 'vue'
import { balancedCardRows } from '../utils/cardGrid'
import { toCssLength } from '../utils/cssLength'

/// 指标卡片容器：按实际宽度算出「一行放几张」，再把 count 张卡片均摊到各行。
///
/// 布局交给外层、卡片本体不管宽度 —— 所以调用方必须用 scoped slot 拿到的
/// cellStyle(index) 设到每个卡片上，漏掉这一步所有卡片会挤成一列。
const props = withDefaults(
  defineProps<{ count: number; minCardWidth?: number; gap?: number | string }>(),
  { minCardWidth: 320, gap: 'var(--lg-s7)' },
)

const host = ref<HTMLElement>()
const width = ref(0)
const measuredGap = ref(16)
/// 当前 --k。卡片内的一切尺寸（字号、留白、图表高度）都按 --k 缩放，
/// 卡片宽度也必须按同一个系数缩放，否则「文字变大、卡片不变」，
/// 大屏上一行挤下 4 张窄卡、小屏上一行只放得下 2 张超大卡。
/// 从自身的计算样式读：--k 写在 :root 上，任何元素都能取到同一个值
const scale = ref(1)
let resize: ResizeObserver | undefined
let measureFrame: number | undefined

/// minCardWidth 的口径是「k=1 时的参考宽度」，实际阈值要乘上 --k
const minimum = computed(() => {
  const base =
    Number.isFinite(props.minCardWidth) && props.minCardWidth > 0 ? props.minCardWidth : 320
  return base * scale.value
})
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

/// 间距与 --k 都取自计算样式而不是 props：props 给的是 var(--lg-s7) 这种令牌，
/// 要让算式参与计算必须先解析成数字
function measure() {
  if (!host.value) return
  measuredGap.value = Number.parseFloat(getComputedStyle(host.value).columnGap) || 0
  const parsed = Number.parseFloat(getComputedStyle(host.value).getPropertyValue('--k'))
  scale.value = Number.isFinite(parsed) && parsed > 0 ? parsed : 1
}

watch(() => props.gap, measure, { flush: 'post' })

onMounted(() => {
  if (!host.value) return
  resize = new ResizeObserver((entries) => {
    const entry = entries[0]
    if (!entry) return
    if (measureFrame !== undefined) cancelAnimationFrame(measureFrame)
    // 在下一帧再发布尺寸：在 observer 回调里直接改响应式数据会触发
    // "ResizeObserver loop completed with undelivered notifications"
    measureFrame = requestAnimationFrame(() => {
      measureFrame = undefined
      width.value = entry.contentRect.width
      measure()
    })
  })
  resize.observe(host.value)
  width.value = host.value.clientWidth
  // --k 由根组件写入，Vue 的 mounted 是子先父后，这一帧读到的可能还是
  // theme.css 里的静态回退值 1，所以下一帧再补一次
  measure()
  measureFrame = requestAnimationFrame(() => {
    measureFrame = undefined
    measure()
  })
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
