<script setup lang="ts" generic="T">
import DataTable from 'primevue/datatable'
import type { DataTableSlots } from 'primevue/datatable'
import { computed } from 'vue'

defineOptions({ inheritAttrs: false })
const props = withDefaults(defineProps<{ value: readonly T[]; minWidth?: string | number }>(), {
  minWidth: 0,
})
const rows = computed(() => [...props.value])
const width = computed(() =>
  typeof props.minWidth === 'number' ? `${props.minWidth}px` : props.minWidth,
)
defineSlots<DataTableSlots<T>>()
</script>
<template>
  <DataTable
    v-bind="$attrs"
    :value="rows"
    class="lumina-table"
    :style="{ '--lumina-table-min-width': width }"
  >
    <template v-for="(_, name) in $slots" #[name]="slotProps">
      <slot :name="name as keyof DataTableSlots<T>" v-bind="slotProps ?? {}" />
    </template>
  </DataTable>
</template>
