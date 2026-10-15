<script setup lang="ts">
import { computed } from 'vue'
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
import { formatTime } from '../domain'
import type { MachineMetric } from '../api/types'

const { t, locale } = useI18n()
const { snapshot, visibleSamples, error, paused, hidden, windowSeconds, now, stale } =
  usePerformance()
const windows = computed(() =>
  [300, 900].map((value) => ({ value, label: t('performance.window', { minutes: value / 60 }) })),
)
const numberFormatter = computed(
  () => new Intl.NumberFormat(locale.value, { maximumFractionDigits: 1 }),
)
const timeFormatter = computed(
  () => new Intl.DateTimeFormat(locale.value, { hour: '2-digit', minute: '2-digit' }),
)
const percent = (value: number | null | undefined) =>
  value == null ? '—' : `${numberFormatter.value.format(value)}%`
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
const timeAxisLabel = computed(() => {
  const formatter = timeFormatter.value
  return (value: number) => formatter.format(value)
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
function trend(key: 'cpu' | 'memory'): LuminaChartOption {
  return {
    grid: { left: 36, right: 12, top: 12, bottom: 26 },
    tooltip: {
      trigger: 'axis',
      valueFormatter: percentTooltip.value,
    },
    xAxis: {
      type: 'time',
      axisLine: { show: false },
      axisTick: { show: false },
      axisLabel: {
        color: '#8993b2',
        formatter: timeAxisLabel.value,
      },
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
        smooth: false,
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
const cardCount = computed(() => 2 + (snapshot.value?.disks.length ?? 0))
const memoryOption = computed(() => trend('memory'))
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
      :style="cellStyle(0)"
      :title="t('performance.cpu')"
      :subtitle="t('performance.cores', { count: snapshot?.logical_cpus ?? '—' })"
      :state="state(snapshot?.cpu)"
      :state-label="stateLabel(snapshot?.cpu)"
    >
      <div class="performance-value">{{ percent(snapshot?.cpu.usage_percent) }}</div>
      <LuminaChart :option="cpuOption" :label="t('performance.cpuTrend')" />
      <template #footer>{{ t('performance.sampleEvery', { seconds: 2 }) }}</template>
    </LuminaCard>
    <LuminaCard
      :style="cellStyle(1)"
      :title="t('performance.memory')"
      :subtitle="t('performance.physicalMemory')"
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
      v-for="(disk, index) in snapshot?.disks ?? []"
      :style="cellStyle(index + 2)"
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
        <span v-if="now - disk.sampled_at > 65_000" class="performance-stale"
          >{{ t('performance.stale') }} ·
        </span>
        {{ t('performance.updated') }}: {{ formatTime(disk.sampled_at) }} ·
        {{ t('performance.sampleEvery', { seconds: 30 }) }}
      </template>
    </LuminaCard>
  </LuminaCardGrid>
  <p v-if="snapshot && !snapshot.disks.length" class="hint">{{ t('performance.noDisks') }}</p>
</template>
<style scoped>
.performance-controls {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: var(--lg-s4);
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
