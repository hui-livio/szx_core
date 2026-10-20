<script setup lang="ts">
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'
import type { MachineSnapshot } from '../../api/types'
import { LuminaChart, type LuminaChartOption } from '../../ui/lumina'
import { formatBytes } from './model'

defineProps<{ snapshot?: MachineSnapshot; gpuOption: LuminaChartOption }>()

const { t, locale } = useI18n()
const formatter = computed(() => new Intl.NumberFormat(locale.value, { maximumFractionDigits: 1 }))
const percent = (value: number | null | undefined) =>
  value == null ? '—' : `${formatter.value.format(value)}%`
const bytes = (value: number | null | undefined) =>
  formatBytes(value ?? null, locale.value, formatter.value)
const speed = (value: number | null | undefined) => (value == null ? '—' : `${bytes(value)}/s`)
const number = (value: number | null | undefined) =>
  value == null ? '—' : formatter.value.format(value)
</script>

<template>
  <template v-if="snapshot">
    <section class="hardware-section" aria-labelledby="performance-gpu-title">
      <header class="hardware-heading">
        <h2 id="performance-gpu-title">{{ t('performance.gpu') }}</h2>
        <span v-if="snapshot.gpu_count"
          >{{ snapshot.gpu_name || t('performance.gpu') }} ·
          {{ t('performance.gpuFirst', { count: snapshot.gpu_count }) }}</span
        >
      </header>
      <template v-if="snapshot.gpu_count">
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
      </template>
      <p v-else class="hardware-empty">{{ t('performance.noGpu') }}</p>
    </section>

    <section class="hardware-section" aria-labelledby="performance-network-title">
      <header class="hardware-heading">
        <h2 id="performance-network-title">{{ t('performance.network') }}</h2>
      </header>
      <div
        v-if="snapshot.networks?.length"
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
            <tr v-for="network in snapshot.networks" :key="network.id">
              <th scope="row">
                <strong>{{ network.name || network.id }}</strong
                ><small>{{ network.description }}</small>
              </th>
              <td class="network-address">
                {{ [network.ipv4, network.ipv6].filter(Boolean).join('\n') || '—' }}
              </td>
              <td class="numeric">{{ speed(network.rx_bytes_per_sec) }}</td>
              <td class="numeric">{{ speed(network.tx_bytes_per_sec) }}</td>
              <td class="numeric">
                {{ network.link_mbps == null ? '—' : `${number(network.link_mbps)} Mbps` }}
              </td>
            </tr>
          </tbody>
        </table>
      </div>
      <p v-else class="hardware-empty">{{ t('performance.noNetworks') }}</p>
    </section>

    <section
      v-if="snapshot.processes?.length"
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
              <th scope="row">{{ process.name }}</th>
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
