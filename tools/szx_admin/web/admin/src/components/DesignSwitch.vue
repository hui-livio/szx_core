<script setup lang="ts">
import { computed, shallowRef } from 'vue'
import { useI18n } from 'vue-i18n'
import { LuminaButton, LuminaPopupMenu } from '../ui/lumina'
import { useUiPreferencesStore, type UiDesign } from '../stores/uiPreferences'

const { t } = useI18n()
const preferences = useUiPreferencesStore()
const menu = shallowRef<InstanceType<typeof LuminaPopupMenu>>()
const open = shallowRef(false)
const choices: UiDesign[] = ['glass', 'industrial']
const items = computed(() =>
  choices.map((value) => ({
    label: t(`appearance.${value}`),
    selected: preferences.design === value,
    command: () => preferences.setDesign(value),
  })),
)
</script>
<template>
  <LuminaButton
    class="lumina-preference-button design-trigger"
    :aria-label="t('appearance.design')"
    :title="t('appearance.design')"
    aria-haspopup="menu"
    :aria-expanded="open"
    @click="menu?.toggle($event)"
  >
    <svg
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      stroke-width="2"
      shape-rendering="geometricPrecision"
      aria-hidden="true"
    >
      <rect x="3" y="3" width="18" height="18" rx="4" />
      <path d="M3 9h18M9 9v12" />
    </svg>
  </LuminaButton>
  <LuminaPopupMenu ref="menu" :model="items" @show="open = true" @hide="open = false">
    <template #item="{ item, props }">
      <a v-bind="props.action" :class="{ 'design-selected': item.selected }">{{ item.label }}</a>
    </template>
  </LuminaPopupMenu>
</template>
<style scoped>
.design-trigger {
  flex: none;
}
.design-selected {
  color: var(--accent);
  font-weight: 600;
}
</style>
