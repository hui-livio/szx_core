<script setup lang="ts" generic="T">
import Select from 'primevue/select'
import { computed } from 'vue'
import type { SelectSlots } from 'primevue/select'

defineOptions({ inheritAttrs: false })
const model = defineModel<T>({ required: true })
const props = defineProps<{ options: readonly unknown[] }>()
const options = computed(() => [...props.options])
defineSlots<SelectSlots>()
</script>
<template>
  <Select v-bind="$attrs" v-model="model" :options="options">
    <template v-for="(_, name) in $slots" #[name]="slotProps">
      <slot :name="name as keyof SelectSlots" v-bind="slotProps ?? {}" />
    </template>
  </Select>
</template>
