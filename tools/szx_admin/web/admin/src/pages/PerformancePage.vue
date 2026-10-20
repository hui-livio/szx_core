<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, shallowRef } from 'vue'
import { useI18n } from 'vue-i18n'
import {
  LuminaButton as Button,
  LuminaCard,
  LuminaCardGrid,
  LuminaChart,
  LuminaSelect as Select,
  type LuminaChartOption,
} from '../ui/lumina'
import { usePerformance } from '../features/performance/usePerformance'
import { formatBytes } from '../features/performance/model'
import PerformanceHardware from '../features/performance/PerformanceHardware.vue'
import { usePerformanceCards } from '../features/performance/usePerformanceCards'
import { formatTime } from '../domain'
import type { MachineMetric } from '../api/types'

const { t, locale } = useI18n()
const {
  snapshot,
  visibleSamples,
  visibleNetworkSamples,
  error,
  paused,
  hidden,
  windowSeconds,
  now,
  stale,
} = usePerformance()
const { hiddenCards, isVisible, toggleCard } = usePerformanceCards()
const cardsMenuOpen = shallowRef(false)
const cardsHost = shallowRef<HTMLElement>()
const visibleDisks = computed(() =>
  (snapshot.value?.disks ?? []).filter((disk) => isVisible(`disk:${disk.id}`)),
)
const cardKeys = computed(() => [
  ...(isVisible('cpu') ? ['cpu'] : []),
  ...(isVisible('memory') ? ['memory'] : []),
  ...visibleDisks.value.map((disk) => `disk:${disk.id}`),
])
const cardIndex = (key: string) => cardKeys.value.indexOf(key)
const displayItems = computed(() => [
  { key: 'cpu', label: t('performance.cpu') },
  { key: 'memory', label: t('performance.memory') },
  ...(snapshot.value?.gpu_count ? [{ key: 'gpu', label: t('performance.gpu') }] : []),
  ...(snapshot.value?.disks ?? []).map((disk) => ({
    key: `disk:${disk.id}`,
    label: `${t('performance.disk')} ${disk.path}`,
  })),
  ...(snapshot.value?.networks ?? []).map((network) => ({
    key: `net:${network.id}`,
    label: network.name || network.id,
  })),
  ...(snapshot.value?.processes?.length
    ? [{ key: 'processes', label: t('performance.processes') }]
    : []),
])
function closeCardsMenu(event: PointerEvent) {
  if (cardsMenuOpen.value && !cardsHost.value?.contains(event.target as Node))
    cardsMenuOpen.value = false
}
function closeCardsMenuOnEscape(event: KeyboardEvent) {
  if (event.key === 'Escape') cardsMenuOpen.value = false
}
onMounted(() => {
  document.addEventListener('pointerdown', closeCardsMenu)
  document.addEventListener('keydown', closeCardsMenuOnEscape)
})
onBeforeUnmount(() => {
  document.removeEventListener('pointerdown', closeCardsMenu)
  document.removeEventListener('keydown', closeCardsMenuOnEscape)
})
const windows = computed(() =>
  [300, 900].map((value) => ({ value, label: t('performance.window', { minutes: value / 60 }) })),
)
const numberFormatter = computed(
  () => new Intl.NumberFormat(locale.value, { maximumFractionDigits: 1 }),
)
const percent = (value: number | null | undefined) =>
  value == null || !Number.isFinite(value) ? '—' : `${numberFormatter.value.format(value)}%`
const bytes = (value: number | null | undefined) =>
  formatBytes(value ?? null, locale.value, numberFormatter.value)
// Stable callback identities let the public chart detect equivalent options.
const percentTooltip = computed(() => {
  const formatter = numberFormatter.value
  return (value: unknown) => (typeof value === 'number' ? `${formatter.format(value)}%` : '—')
})
const bytesTooltip = computed(() => {
  const currentLocale = locale.value
  const formatter = numberFormatter.value
  return (value: unknown) =>
    formatBytes(typeof value === 'number' ? value : null, currentLocale, formatter)
})
const state = (metric?: MachineMetric) =>
  !metric
    ? 'loading'
    : metric.status === 'ok'
      ? 'ready'
      : metric.status === 'warming_up'
        ? 'loading'
        : 'unavailable'
const stateLabel = (metric?: MachineMetric) =>
  t(
    !metric
      ? 'common.loading'
      : metric.status === 'warming_up'
        ? 'performance.warming'
        : 'performance.unavailable',
  )
function trend(key: 'cpu' | 'memory' | 'gpu'): LuminaChartOption {
  return {
    grid: { left: 36, right: 12, top: 12, bottom: 10 },
    tooltip: {
      trigger: 'axis',
      valueFormatter: percentTooltip.value,
    },
    xAxis: {
      type: 'time',
      axisLine: { show: false },
      axisTick: { show: false },
      axisLabel: { show: false },
    },
    yAxis: {
      type: 'value',
      min: 0,
      max: 100,
      splitNumber: 2,
      axisLabel: { color: '#8993b2', formatter: '{value}%' },
      splitLine: { lineStyle: { color: 'rgba(137,147,178,0.15)' } },
    },
    series: [
      {
        type: 'line',
        name: t(`performance.${key}`),
        showSymbol: false,
        connectNulls: false,
        smooth: true,
        areaStyle: { opacity: 0.12 },
        lineStyle: { width: 2 },
        data: visibleSamples.value.map((sample) => [
          sample.sampled_at,
          sample[key].status === 'ok' ? sample[key].usage_percent : null,
        ]),
      },
    ],
  }
}
const cpuOption = computed(() => trend('cpu'))
const cardCount = computed(() => cardKeys.value.length)
const memoryOption = computed(() => trend('memory'))
const gpuOption = computed(() => trend('gpu'))
interface DiskChart {
  used: number
  free: number
  locale: string
  option: LuminaChartOption
}
const diskOptions = computed((previous: Map<string, DiskChart> | undefined) => {
  const disks = snapshot.value?.disks ?? []
  const currentLocale = locale.value
  let unchanged = previous?.size === disks.length
  const next = new Map<string, DiskChart>()
  for (const disk of disks) {
    const used = disk.capacity.used_bytes ?? 0
    const free = disk.capacity.free_bytes ?? 0
    const old = previous?.get(disk.id)
    if (old && old.used === used && old.free === free && old.locale === currentLocale) {
      next.set(disk.id, old)
    } else {
      unchanged = false
      next.set(disk.id, { used, free, locale: currentLocale, option: capacity(disk.capacity) })
    }
  }
  return unchanged && previous ? previous : next
})
function capacity(metric: MachineMetric): LuminaChartOption {
  return {
    tooltip: {
      trigger: 'item',
      valueFormatter: bytesTooltip.value,
    },
    series: [
      {
        type: 'pie',
        radius: ['63%', '82%'],
        center: ['50%', '50%'],
        label: { show: false },
        emphasis: { scale: false },
        data: [
          { name: t('performance.used'), value: metric.used_bytes ?? 0 },
          {
            name: t('performance.free'),
            value: metric.free_bytes ?? 0,
            itemStyle: { color: 'rgba(137,147,178,0.2)' },
          },
        ],
      },
    ],
  }
}
</script>
<template>
  <header class="page-heading">
    <div>
      <h1>{{ t('performance.title') }}</h1>
      <p>{{ t('performance.subtitle') }}</p>
    </div>
    <div class="performance-controls">
      <Select
        v-model="windowSeconds"
        :options="windows"
        option-label="label"
        option-value="value"
        :aria-label="t('performance.historyWindow')"
      />
      <Button type="button" :aria-pressed="paused" @click="paused = !paused">{{
        t(paused ? 'performance.resume' : 'performance.pause')
      }}</Button>
      <div ref="cardsHost" class="performance-cards-host">
        <Button
          type="button"
          :aria-label="t('performance.visibleCards')"
          :title="t('performance.visibleCards')"
          :aria-expanded="cardsMenuOpen"
          aria-haspopup="true"
          @click="cardsMenuOpen = !cardsMenuOpen"
        >
          <svg
            viewBox="0 0 24 24"
            fill="none"
            stroke="currentColor"
            stroke-width="2"
            aria-hidden="true"
          >
            <circle cx="12" cy="12" r="3" />
            <path
              d="M19 13.5a7 7 0 0 0 0-3l2-1.5-2-3.4-2.3 1a7 7 0 0 0-2.6-1.5L13.8 2h-4l-.3 3.1a7 7 0 0 0-2.6 1.5l-2.3-1-2 3.4 2 1.5a7 7 0 0 0 0 3l-2 1.5 2 3.4 2.3-1a7 7 0 0 0 2.6 1.5l.3 3.1h4l.3-3.1a7 7 0 0 0 2.6-1.5l2.3 1 2-3.4Z"
            />
          </svg>
        </Button>
        <div
          v-if="cardsMenuOpen"
          class="performance-cards-menu"
          role="group"
          :aria-label="t('performance.visibleCards')"
        >
          <label v-for="item in displayItems" :key="item.key">
            <input type="checkbox" :checked="isVisible(item.key)" @change="toggleCard(item.key)" />
            <span>{{ item.label }}</span>
          </label>
        </div>
      </div>
    </div>
  </header>
  <p v-if="error" class="alert error" role="alert">{{ error }}</p>
  <div class="performance-meta" role="status">
    <span>{{ snapshot?.platform.toUpperCase() ?? '—' }} · {{ t('performance.source') }}</span>
    <span>{{ t('performance.updated') }}: {{ formatTime(snapshot?.sampled_at ?? null) }}</span>
    <span v-if="paused || hidden || stale" class="performance-stale">{{
      t(paused || hidden ? 'performance.paused' : 'performance.stale')
    }}</span>
  </div>
  <LuminaCardGrid
    v-slot="{ cellStyle }"
    class="performance-grid"
    :count="cardCount"
    :aria-label="t('performance.title')"
  >
    <LuminaCard
      v-if="isVisible('cpu')"
      variant="glass"
      :style="cellStyle(cardIndex('cpu'))"
      :title="t('performance.cpu')"
      :subtitle="
        snapshot?.cpu_name || t('performance.cores', { count: snapshot?.logical_cpus ?? '—' })
      "
      :state="state(snapshot?.cpu)"
      :state-label="stateLabel(snapshot?.cpu)"
    >
      <div class="performance-value">{{ percent(snapshot?.cpu.usage_percent) }}</div>
      <LuminaChart :option="cpuOption" :label="t('performance.cpuTrend')" />
      <dl class="performance-details">
        <div>
          <dt>{{ t('performance.coresLabel') }}</dt>
          <dd>{{ snapshot?.logical_cpus ?? '—' }}</dd>
        </div>
        <div>
          <dt>{{ t('performance.temperature') }}</dt>
          <dd>
            {{
              snapshot?.cpu.temperature_celsius == null
                ? '—'
                : `${numberFormatter.format(snapshot.cpu.temperature_celsius)} °C`
            }}
          </dd>
        </div>
      </dl>
      <template #footer>{{ t('performance.sampleEvery', { seconds: 2 }) }}</template>
    </LuminaCard>
    <LuminaCard
      v-if="isVisible('memory')"
      variant="glass"
      :style="cellStyle(cardIndex('memory'))"
      :title="t('performance.memory')"
      :subtitle="snapshot?.memory_spec || t('performance.physicalMemory')"
      :state="state(snapshot?.memory)"
      :state-label="stateLabel(snapshot?.memory)"
    >
      <div class="performance-value">{{ percent(snapshot?.memory.usage_percent) }}</div>
      <LuminaChart :option="memoryOption" :label="t('performance.memoryTrend')" />
      <dl class="performance-details">
        <div>
          <dt>{{ t('performance.total') }}</dt>
          <dd>{{ bytes(snapshot?.memory.total_bytes) }}</dd>
        </div>
        <div>
          <dt>{{ t('performance.used') }}</dt>
          <dd>{{ bytes(snapshot?.memory.used_bytes) }}</dd>
        </div>
        <div>
          <dt>{{ t('performance.available') }}</dt>
          <dd>{{ bytes(snapshot?.memory.available_bytes) }}</dd>
        </div>
      </dl>
      <template #footer>{{ t('performance.availableHint') }}</template>
    </LuminaCard>
    <LuminaCard
      variant="glass"
      v-for="disk in visibleDisks"
      :style="cellStyle(cardIndex(`disk:${disk.id}`))"
      :key="disk.id"
      :title="t('performance.disk')"
      :subtitle="disk.path"
      :state="state(disk.capacity)"
      :state-label="stateLabel(disk.capacity)"
    >
      <div class="performance-value">{{ percent(disk.capacity.usage_percent) }}</div>
      <LuminaChart
        :option="diskOptions.get(disk.id)!.option"
        :label="t('performance.diskCapacity', { path: disk.path })"
      />
      <dl class="performance-details">
        <div>
          <dt>{{ t('performance.total') }}</dt>
          <dd>{{ bytes(disk.capacity.total_bytes) }}</dd>
        </div>
        <div>
          <dt>{{ t('performance.used') }}</dt>
          <dd>{{ bytes(disk.capacity.used_bytes) }}</dd>
        </div>
        <div>
          <dt>{{ t('performance.free') }}</dt>
          <dd>{{ bytes(disk.capacity.free_bytes) }}</dd>
        </div>
        <div>
          <dt>{{ t('performance.available') }}</dt>
          <dd>{{ bytes(disk.capacity.available_bytes) }}</dd>
        </div>
      </dl>
      <template #footer>
        <span v-if="disk.type">{{ disk.type }} · </span>
        <span v-if="now - disk.sampled_at > 65_000" class="performance-stale"
          >{{ t('performance.stale') }} ·
        </span>
        {{ t('performance.updated') }}: {{ formatTime(disk.sampled_at) }} ·
        {{ t('performance.sampleEvery', { seconds: 30 }) }}
      </template>
    </LuminaCard>
  </LuminaCardGrid>
  <p v-if="snapshot && !snapshot.disks.length" class="hint">{{ t('performance.noDisks') }}</p>
  <PerformanceHardware
    :snapshot="snapshot"
    :gpu-option="gpuOption"
    :network-samples="visibleNetworkSamples"
    :hidden-cards="hiddenCards"
  />
</template>
<style scoped>
.performance-controls {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: var(--lg-s4);
}
.performance-cards-host {
  position: relative;
}
.performance-cards-host svg {
  width: 18px;
  height: 18px;
}
.performance-cards-menu {
  position: absolute;
  z-index: 20;
  inset-inline-end: 0;
  inset-block-start: calc(100% + var(--lg-s3));
  min-width: min(280px, 80vw);
  max-height: min(400px, 70vh);
  overflow: auto;
  padding: var(--lg-s3);
  background: var(--surface);
  border: 1px solid var(--line);
  border-radius: var(--lg-r-lg);
  box-shadow: var(--lg-sh-card);
}
.performance-cards-menu label {
  display: flex;
  flex-direction: row;
  align-items: center;
  gap: var(--lg-s4);
  padding: var(--lg-s3);
  overflow-wrap: anywhere;
}
.performance-cards-menu input {
  width: auto;
  flex: none;
}
.performance-meta {
  display: flex;
  flex-wrap: wrap;
  gap: var(--lg-s5);
  color: var(--muted);
  font-size: var(--lg-fs-sm);
  margin-bottom: var(--lg-s7);
}
.performance-grid :deep(.lumina-card-heading p) {
  /* Reserve two subtitle lines so short and long mount names stay aligned. */
  line-height: 1.4;
  min-height: 2.8em;
}
.performance-value {
  font-size: clamp(30px, 3vw, 42px);
  font-weight: 500;
  color: var(--text);
  font-variant-numeric: tabular-nums;
}
.performance-details {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: var(--lg-s4);
  margin: var(--lg-s5) 0 0;
}
.performance-details dt {
  color: var(--muted);
  font-size: var(--lg-fs-sm);
}
.performance-details dd {
  margin: var(--lg-s2) 0 0;
  font-variant-numeric: tabular-nums;
}
.performance-stale {
  color: var(--lg-warning);
}
</style>
