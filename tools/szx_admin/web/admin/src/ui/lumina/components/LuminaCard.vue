<script setup lang="ts">
import LuminaPanel from './LuminaPanel.vue'

withDefaults(
  defineProps<{
    title?: string
    subtitle?: string
    variant?: 'glass' | 'solid'
    radius?: number | string
    padding?: number | string
    state?: 'ready' | 'loading' | 'unavailable'
    stateLabel?: string
  }>(),
  { variant: 'solid', radius: 'var(--lg-r-3xl)', padding: 'var(--lg-s9)', state: 'ready' },
)
</script>
<template>
  <LuminaPanel
    as="article"
    class="lumina-card lumina-smooth-edge"
    :variant="variant"
    :radius="radius"
    :padding="padding"
    :aria-busy="state === 'loading'"
  >
    <header v-if="title || $slots.header || $slots.actions" class="lumina-card-header">
      <slot name="header">
        <div class="lumina-card-heading">
          <h2>{{ title }}</h2>
          <p v-if="subtitle">{{ subtitle }}</p>
        </div>
      </slot>
      <div v-if="$slots.actions" class="lumina-card-actions"><slot name="actions" /></div>
    </header>
    <div class="lumina-card-body">
      <div v-if="state !== 'ready'" class="lumina-card-state" role="status">
        <slot name="state" :state="state">{{ stateLabel }}</slot>
      </div>
      <slot v-else />
    </div>
    <footer v-if="$slots.footer" class="lumina-card-footer"><slot name="footer" /></footer>
  </LuminaPanel>
</template>
