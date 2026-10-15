<script setup lang="ts">
import { computed, ref, useId } from 'vue'
import Menu from 'primevue/menu'
import type { MenuItem } from 'primevue/menuitem'
import type { MenuSlots } from 'primevue/menu'
import { alignLuminaPopupArrow } from '../utils/popupArrow'

defineOptions({ inheritAttrs: false })
const props = defineProps<{ id?: string; model: readonly MenuItem[] }>()
const emit = defineEmits<{ show: []; hide: [] }>()
defineSlots<MenuSlots>()
const fallbackId = useId()
const menuId = computed(() => props.id ?? fallbackId)
const model = computed(() => [...props.model])
const menu = ref<InstanceType<typeof Menu>>()
let anchor: HTMLElement | undefined

function toggle(event: Event, target?: HTMLElement) {
  anchor = target ?? (event.currentTarget as HTMLElement)
  menu.value?.toggle(event, anchor)
}

function show(event: Event, target?: HTMLElement) {
  anchor = target ?? (event.currentTarget as HTMLElement)
  menu.value?.show(event, anchor)
}

function hide() {
  menu.value?.hide()
}

function onShow() {
  // PrimeVue has positioned the portal before emitting show. Match its collision
  // placement so the arrow still points at the trigger near viewport edges.
  const popup = document.getElementById(menuId.value)
  if (popup && anchor) alignLuminaPopupArrow(popup, anchor)
  emit('show')
}

defineExpose({ toggle, show, hide })
</script>
<template>
  <Menu
    ref="menu"
    v-bind="$attrs"
    :id="menuId"
    class="lumina-popup-menu"
    :model="model"
    popup
    @show="onShow"
    @hide="emit('hide')"
  >
    <template v-for="(_, name) in $slots" #[name]="slotProps">
      <slot :name="name as keyof MenuSlots" v-bind="slotProps ?? {}" />
    </template>
  </Menu>
</template>
