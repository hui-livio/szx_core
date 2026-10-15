<script setup lang="ts" generic="T">
import DataTable from 'primevue/datatable'
import type { DataTableSlots } from 'primevue/datatable'
import { computed } from 'vue'
import { createStableRows } from '../utils/stableRows'

defineOptions({ inheritAttrs: false })
const props = withDefaults(
  defineProps<{
    value: readonly T[]
    minWidth?: string | number
    dataKey?: string | number | symbol | ((row: T) => string)
  }>(),
  {
    minWidth: 0,
  },
)
const stabilize = createStableRows<T>()
const rows = computed(() => stabilize(props.value, props.dataKey))
const width = computed(() =>
  typeof props.minWidth === 'number' ? `${props.minWidth}px` : props.minWidth,
)
defineSlots<DataTableSlots<T>>()
</script>
<template>
  <DataTable
    v-bind="$attrs"
    :value="rows"
    :data-key="dataKey"
    class="lumina-table"
    :style="{ '--lumina-table-min-width': width }"
  >
    <template v-for="(_, name) in $slots" #[name]="slotProps">
      <slot :name="name as keyof DataTableSlots<T>" v-bind="slotProps ?? {}" />
    </template>
  </DataTable>
</template>
