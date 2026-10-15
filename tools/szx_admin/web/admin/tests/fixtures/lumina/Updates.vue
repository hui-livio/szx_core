<script setup lang="ts">
import { onMounted, ref } from 'vue'
import { getInstanceByDom } from 'echarts/core'
import {
  LuminaButton,
  LuminaChart,
  LuminaTable,
  LuminaColumn,
  type LuminaChartOption,
} from '../../../src/ui/lumina'

interface Row {
  id: string
  name: string
  meta: { enabled: boolean }
}
const rows = ref<Row[]>([
  { id: 'a', name: 'Alpha', meta: { enabled: true } },
  { id: 'b', name: 'Beta', meta: { enabled: false } },
])
const heading = ref('Name')
const picked = ref('')
const busy = ref(false)
const label = ref('Update chart')
const formatter = (value: unknown) => `${value}%`
const makeOption = (data = [1, 2]): LuminaChartOption => ({
  xAxis: { type: 'category', data: ['A', 'B'] },
  yAxis: { type: 'value' },
  tooltip: { valueFormatter: formatter },
  series: [{ type: 'line', data }],
})
const option = ref<LuminaChartOption>(makeOption())

function equivalent() {
  rows.value = rows.value.map((row) => ({ ...row, meta: { ...row.meta } }))
  option.value = makeOption()
}
function changed() {
  rows.value = rows.value.map((row) => (row.id === 'a' ? { ...row, name: 'Changed' } : { ...row }))
  option.value = makeOption([3, 2])
}
function mutate() {
  rows.value[0]!.name = 'Mutated'
  rows.value[0]!.meta.enabled = !rows.value[0]!.meta.enabled
  const series = option.value.series as { data: number[] }[]
  series[0]!.data[0] = 9
}
function changeFormatter() {
  option.value = {
    ...option.value,
    tooltip: { valueFormatter: (value: unknown) => `New ${value}` },
  }
}
function appearance(attribute: 'theme' | 'fx') {
  const root = document.documentElement
  root.dataset[attribute] =
    attribute === 'theme'
      ? root.dataset.theme === 'dark'
        ? 'light'
        : 'dark'
      : root.dataset.fx === 'full'
        ? 'lite'
        : 'full'
}

// Browser regression counters instrument the real ECharts methods, not a mock.
onMounted(() => {
  const host = document.getElementById('update-chart')!
  const chart = getInstanceByDom(host)!
  host.dataset.updates = '0'
  host.dataset.resizes = '0'
  chart.setOption = new Proxy(chart.setOption, {
    apply(target, instance, args) {
      host.dataset.updates = String(Number(host.dataset.updates) + 1)
      return Reflect.apply(target, instance, args)
    },
  })
  chart.resize = new Proxy(chart.resize, {
    apply(target, instance, args) {
      host.dataset.resizes = String(Number(host.dataset.resizes) + 1)
      return Reflect.apply(target, instance, args)
    },
  })
})
</script>
<template>
  <main>
    <div class="actions">
      <LuminaButton label="Equivalent data" @click="equivalent" />
      <LuminaButton label="Changed data" @click="changed" />
      <LuminaButton label="Mutate data" @click="mutate" />
      <LuminaButton label="Change formatter" @click="changeFormatter" />
      <LuminaButton label="Rename chart" @click="label = 'Renamed chart'" />
      <LuminaButton label="Reverse rows" @click="rows = [...rows].reverse()" />
      <LuminaButton label="Remove row" @click="rows = rows.slice(1)" />
      <LuminaButton
        label="Add row"
        @click="rows = [...rows, { id: 'c', name: 'Gamma', meta: { enabled: true } }]"
      />
      <LuminaButton label="Clear rows" @click="rows = []" />
      <LuminaButton label="Rename column" @click="heading = 'Localized name'" />
      <LuminaButton label="Toggle busy" @click="busy = !busy" />
      <LuminaButton label="Toggle theme" @click="appearance('theme')" />
      <LuminaButton label="Toggle effects" @click="appearance('fx')" />
    </div>
    <LuminaTable id="update-table" :value="rows" data-key="id" :aria-busy="busy">
      <LuminaColumn field="name" :header="heading" sortable>
        <template #body="{ data }: { data: Row }"
          ><LuminaButton :label="data.name" @click="picked = `${data.id}:${data.name}`"
        /></template>
      </LuminaColumn>
      <LuminaColumn header="Enabled"
        ><template #body="{ data }: { data: Row }">{{ data.meta.enabled }}</template></LuminaColumn
      >
      <template #empty>No rows</template>
    </LuminaTable>
    <output data-testid="picked">{{ picked }}</output>
    <div id="chart-container">
      <LuminaChart id="update-chart" :option="option" :label="label" />
    </div>
  </main>
</template>
<style scoped>
main {
  margin: 20px;
  max-width: 800px;
}
.actions {
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
  margin-bottom: 20px;
}
#chart-container {
  width: 100%;
}
</style>
