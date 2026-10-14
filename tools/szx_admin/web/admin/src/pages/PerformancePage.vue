<script setup lang="ts">
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'
import {
  LuminaButton as Button,
  LuminaCard,
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
const percent = (value: number | null | undefined) =>
  value == null
    ? '—'
    : `${new Intl.NumberFormat(locale.value, { maximumFractionDigits: 1 }).format(value)}%`
const bytes = (value: number | null | undefined) => formatBytes(value ?? null, locale.value)
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
      valueFormatter: (value) => percent(typeof value === 'number' ? value : null),
    },
    xAxis: {
      type: 'time',
      axisLine: { show: false },
      axisTick: { show: false },
      axisLabel: {
        color: '#8993b2',
        formatter: (value: number) =>
          new Intl.DateTimeFormat(locale.value, { hour: '2-digit', minute: '2-digit' }).format(
            value,
          ),
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
const memoryOption = computed(() => trend('memory'))
const diskOptions = computed(
  () => new Map((snapshot.value?.disks ?? []).map((disk) => [disk.id, capacity(disk.capacity)])),
)
function capacity(metric: MachineMetric): LuminaChartOption {
  return {
    tooltip: {
      trigger: 'item',
      valueFormatter: (value) => bytes(typeof value === 'number' ? value : null),
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
  <section class="performance-grid" :aria-label="t('performance.title')">
    <LuminaCard
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
      v-for="disk in snapshot?.disks ?? []"
      :key="disk.id"
      :title="t('performance.disk')"
      :subtitle="disk.path"
      :state="state(disk.capacity)"
      :state-label="stateLabel(disk.capacity)"
    >
      <div class="performance-value">{{ percent(disk.capacity.usage_percent) }}</div>
      <LuminaChart
        :option="diskOptions.get(disk.id)!"
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
  </section>
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
.performance-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(min(100%, 320px), 1fr));
  align-items: stretch;
  gap: var(--lg-s7);
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
