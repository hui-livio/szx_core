<script setup lang="ts" generic="T">
import Select from 'primevue/select'
import { computed, useAttrs, useId } from 'vue'
import type { SelectSlots } from 'primevue/select'
import { alignLuminaPopupArrow } from '../utils/popupArrow'

defineOptions({ inheritAttrs: false })
const model = defineModel<T>({ required: true })
const props = defineProps<{ options: readonly unknown[] }>()
const options = computed(() => [...props.options])
defineSlots<SelectSlots>()
const emit = defineEmits<{ show: [] }>()
const attrs = useAttrs()
const fallbackId = useId()
const selectId = computed(() => String(attrs.id ?? fallbackId))

function onShow() {
  const anchor = document.getElementById(selectId.value)
  const listId = anchor?.querySelector('[role="combobox"]')?.getAttribute('aria-controls')
  const popup = listId
    ? document.getElementById(listId)?.closest<HTMLElement>('.p-select-overlay')
    : undefined
  if (popup && anchor) alignLuminaPopupArrow(popup, anchor)
  emit('show')
}
</script>
<template>
  <Select
    :id="selectId"
    class="lumina-smooth-edge"
    v-bind="$attrs"
    v-model="model"
    :options="options"
    @show="onShow"
  >
    <template v-for="(_, name) in $slots" #[name]="slotProps">
      <slot :name="name as keyof SelectSlots" v-bind="slotProps ?? {}" />
    </template>
  </Select>
</template>
