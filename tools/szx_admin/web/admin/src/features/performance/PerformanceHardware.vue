<script setup lang="ts">
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'
import type { MachineNetwork, MachineSnapshot } from '../../api/types'
import { LuminaCard, LuminaCardGrid, LuminaChart, type LuminaChartOption } from '../../ui/lumina'
import { displayMetricText, formatBytes, type NetworkRateSample } from './model'

const props = defineProps<{
  snapshot?: MachineSnapshot
  gpuOption: LuminaChartOption
  networkSamples: NetworkRateSample[]
  hiddenCards: string[]
}>()

const { t, locale } = useI18n()
const formatter = computed(() => new Intl.NumberFormat(locale.value, { maximumFractionDigits: 1 }))
const percent = (value: number | null | undefined) =>
  value == null || !Number.isFinite(value) ? '—' : `${formatter.value.format(value)}%`
const bytes = (value: number | null | undefined) =>
  formatBytes(value ?? null, locale.value, formatter.value)
const speed = (value: number | null | undefined) => (value == null ? '—' : `${bytes(value)}/s`)
const number = (value: number | null | undefined) =>
  value == null || !Number.isFinite(value) ? '—' : formatter.value.format(value)
const linkSpeed = (value: number | null | undefined) =>
  value == null || !Number.isFinite(value) ? '—' : `${number(value)} Mbps`
const addresses = (ipv4: string | null | undefined, ipv6: string | null | undefined) =>
  [ipv4, ipv6]
    .map((address) => displayMetricText(address))
    .filter((address) => address !== '—')
    .join('\n') || '—'
const networkLabel = (network: MachineNetwork) => {
  const name = displayMetricText(network.name)
  return name === '—' ? displayMetricText(network.id) : name
}
const visibleNetworks = computed(() =>
  (props.snapshot?.networks ?? []).filter(
    (network) => !props.hiddenCards.includes(`net:${network.id}`),
  ),
)
function networkChartOption(id: string): LuminaChartOption {
  const points = props.networkSamples
  return {
    grid: { left: 12, right: 12, top: 12, bottom: 8 },
    tooltip: {
      trigger: 'axis',
      valueFormatter: (value: unknown) => (typeof value === 'number' ? speed(value) : '—'),
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
      axisLabel: { show: false },
      splitLine: { lineStyle: { color: 'rgba(137,147,178,0.15)' } },
    },
    series: [
      {
        type: 'line',
        name: t('performance.download'),
        showSymbol: true,
        symbolSize: 5,
        connectNulls: false,
        smooth: true,
        lineStyle: { width: 2, color: '#159d83' },
        itemStyle: { color: '#159d83' },
        data: points.map((sample) => [sample.sampled_at, sample.rates[id]?.rx ?? null]),
      },
      {
        type: 'line',
        name: t('performance.upload'),
        showSymbol: true,
        symbolSize: 5,
        connectNulls: false,
        smooth: true,
        lineStyle: { width: 2, color: '#d18135' },
        itemStyle: { color: '#d18135' },
        data: points.map((sample) => [sample.sampled_at, sample.rates[id]?.tx ?? null]),
      },
    ],
  }
}
const networkChartOptions = computed(
  () =>
    new Map(visibleNetworks.value.map((network) => [network.id, networkChartOption(network.id)])),
)
</script>

<template>
  <template v-if="snapshot">
    <section
      v-if="!hiddenCards.includes('gpu')"
      class="hardware-section"
      :aria-label="t('performance.gpu')"
    >
      <LuminaCard
        v-if="snapshot.gpu_count"
        variant="glass"
        :title="t('performance.gpu')"
        :subtitle="`${snapshot.gpu_name || t('performance.gpu')} · ${t('performance.gpuFirst', { count: snapshot.gpu_count })}`"
      >
        <div class="gpu-layout">
          <div class="gpu-summary">
            <div class="gpu-value">{{ percent(snapshot.gpu.usage_percent) }}</div>
            <span class="gpu-status" v-if="snapshot.gpu.status !== 'ok'">{{
              t(
                snapshot.gpu.status === 'warming_up'
                  ? 'performance.warming'
                  : 'performance.unavailable',
              )
            }}</span>
            <dl class="hardware-facts">
              <div>
                <dt>{{ t('performance.temperature') }}</dt>
                <dd>
                  {{
                    snapshot.gpu.temperature_celsius == null
                      ? '—'
                      : `${number(snapshot.gpu.temperature_celsius)} °C`
                  }}
                </dd>
              </div>
              <div>
                <dt>{{ t('performance.vramUsed') }}</dt>
                <dd>{{ bytes(snapshot.gpu.used_bytes) }}</dd>
              </div>
              <div>
                <dt>{{ t('performance.vramTotal') }}</dt>
                <dd>{{ bytes(snapshot.gpu.total_bytes) }}</dd>
              </div>
              <div>
                <dt>{{ t('performance.cuda') }}</dt>
                <dd>{{ percent(snapshot.gpu_engine.cuda_percent) }}</dd>
              </div>
              <div>
                <dt>{{ t('performance.graphics3d') }}</dt>
                <dd>{{ percent(snapshot.gpu_engine.graphics_3d_percent) }}</dd>
              </div>
            </dl>
          </div>
          <LuminaChart :option="gpuOption" :label="t('performance.gpuTrend')" />
        </div>
      </LuminaCard>
      <p v-else class="hardware-empty">{{ t('performance.noGpu') }}</p>
    </section>

    <section
      v-if="visibleNetworks.length"
      class="hardware-section"
      aria-labelledby="performance-network-title"
    >
      <header class="hardware-heading">
        <h2 id="performance-network-title">{{ t('performance.network') }}</h2>
      </header>
      <LuminaCardGrid v-slot="{ cellStyle }" class="network-grid" :count="visibleNetworks.length">
        <LuminaCard
          v-for="(network, index) in visibleNetworks"
          :key="network.id"
          variant="glass"
          class="network-trend"
          :style="cellStyle(index)"
          :title="networkLabel(network)"
          :subtitle="displayMetricText(network.description)"
        >
          <div class="network-rates">
            <div class="network-rate receive-rate">
              <span>{{ t('performance.download') }}</span>
              <strong>{{ speed(network.rx_bytes_per_sec) }}</strong>
            </div>
            <div class="network-rate send-rate">
              <span>{{ t('performance.upload') }}</span>
              <strong>{{ speed(network.tx_bytes_per_sec) }}</strong>
            </div>
          </div>
          <LuminaChart
            :option="networkChartOptions.get(network.id)!"
            :label="`${t('performance.networkTrend')} · ${networkLabel(network)}`"
          />
        </LuminaCard>
      </LuminaCardGrid>
      <div
        v-if="visibleNetworks.length"
        class="hardware-table-scroll"
        role="region"
        :aria-label="t('performance.network')"
        tabindex="0"
      >
        <table class="hardware-table">
          <thead>
            <tr>
              <th scope="col">{{ t('performance.interface') }}</th>
              <th scope="col">IPv4 / IPv6</th>
              <th scope="col">{{ t('performance.download') }}</th>
              <th scope="col">{{ t('performance.upload') }}</th>
              <th scope="col">{{ t('performance.linkSpeed') }}</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="network in visibleNetworks" :key="network.id">
              <th scope="row">
                <strong>{{ networkLabel(network) }}</strong
                ><small>{{ displayMetricText(network.description) }}</small>
              </th>
              <td class="network-address">{{ addresses(network.ipv4, network.ipv6) }}</td>
              <td class="numeric">{{ speed(network.rx_bytes_per_sec) }}</td>
              <td class="numeric">{{ speed(network.tx_bytes_per_sec) }}</td>
              <td class="numeric">
                {{ linkSpeed(network.link_mbps) }}
              </td>
            </tr>
          </tbody>
        </table>
      </div>
    </section>

    <section
      v-if="snapshot.processes?.length && !hiddenCards.includes('processes')"
      class="hardware-section"
      aria-labelledby="performance-process-title"
    >
      <header class="hardware-heading">
        <h2 id="performance-process-title">{{ t('performance.processes') }}</h2>
      </header>
      <div
        class="hardware-table-scroll"
        role="region"
        :aria-label="t('performance.processes')"
        tabindex="0"
      >
        <table class="hardware-table process-table">
          <thead>
            <tr>
              <th scope="col">{{ t('performance.process') }}</th>
              <th scope="col">{{ t('performance.status') }}</th>
              <th scope="col">{{ t('performance.cpu') }}</th>
              <th scope="col">{{ t('performance.memory') }}</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="process in snapshot.processes" :key="process.name">
              <th scope="row">{{ displayMetricText(process.name) }}</th>
              <td>
                {{
                  t(
                    process.status === 'ok'
                      ? 'performance.running'
                      : process.status === 'stopped'
                        ? 'performance.stopped'
                        : 'performance.unavailable',
                  )
                }}
              </td>
              <td class="numeric">{{ percent(process.cpu_percent) }}</td>
              <td class="numeric">{{ bytes(process.memory_bytes) }}</td>
            </tr>
          </tbody>
        </table>
      </div>
    </section>
  </template>
</template>

<style scoped>
.hardware-section {
  margin-block-start: var(--lg-s10);
}
.hardware-heading {
  display: flex;
  flex-wrap: wrap;
  align-items: baseline;
  gap: var(--lg-s5);
  margin-block-end: var(--lg-s5);
}
.hardware-heading h2 {
  margin: 0;
  font-size: var(--lg-fs-xl);
  font-weight: 600;
}
.network-trend {
  min-width: 0;
}
.network-grid {
  margin-block-end: var(--lg-s6);
}
.network-rates {
  display: flex;
  flex-wrap: wrap;
  gap: var(--lg-s7);
  margin-block-end: var(--lg-s4);
}
.network-rate {
  display: grid;
  gap: var(--lg-s2);
  padding-inline-start: var(--lg-s4);
  border-inline-start: 3px solid;
}
.network-rate span {
  color: var(--muted);
  font-size: var(--lg-fs-sm);
}
.network-rate strong {
  font-variant-numeric: tabular-nums;
  font-size: var(--lg-fs-lg);
}
.receive-rate {
  border-color: #159d83;
}
.send-rate {
  border-color: #d18135;
}
.network-trend .lumina-chart {
  height: 210px;
}
.hardware-heading span,
.hardware-empty,
.gpu-status {
  color: var(--muted);
}
.gpu-layout {
  display: grid;
  grid-template-columns: minmax(230px, 1fr) minmax(0, 2fr);
  align-items: center;
  gap: var(--lg-s7);
}
.gpu-value {
  font-size: 32px;
  font-variant-numeric: tabular-nums;
}
.hardware-facts {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: var(--lg-s5);
  margin: var(--lg-s5) 0 0;
}
.hardware-facts dt {
  color: var(--muted);
  font-size: var(--lg-fs-sm);
}
.hardware-facts dd {
  margin: var(--lg-s2) 0 0;
  font-variant-numeric: tabular-nums;
}
.hardware-table-scroll {
  overflow-x: auto;
  max-inline-size: 100%;
}
.hardware-table {
  width: 100%;
  min-width: 760px;
  border-collapse: collapse;
  text-align: start;
}
.hardware-table th,
.hardware-table td {
  padding: var(--lg-s4) var(--lg-s5);
  border-bottom: 1px solid var(--line);
  text-align: start;
  vertical-align: top;
}
.hardware-table thead th {
  position: sticky;
  top: 0;
  background: var(--surface);
  color: var(--muted);
  font-weight: 500;
}
.hardware-table tbody th {
  font-weight: 500;
  max-width: 240px;
  overflow-wrap: anywhere;
}
.hardware-table small {
  display: block;
  color: var(--muted);
  font-weight: 400;
}
.network-address {
  white-space: pre-line;
  overflow-wrap: anywhere;
}
.hardware-table .numeric {
  white-space: nowrap;
  font-variant-numeric: tabular-nums;
}
.process-table {
  min-width: 520px;
}
@media (max-width: 680px) {
  .gpu-layout {
    grid-template-columns: minmax(0, 1fr);
  }
}
</style>
