<script setup lang="ts">
import { computed } from 'vue'
import { toCssLength } from '../utils/cssLength'

const props = withDefaults(
  defineProps<{
    as?: 'div' | 'section' | 'article'
    variant?: 'glass' | 'solid'
    radius?: number | string
    padding?: number | string
  }>(),
  {
    as: 'div',
    variant: 'glass',
    radius: 'var(--lg-r-4xl)',
    padding: 'var(--lg-pad-card-t) var(--lg-pad-card-x) var(--lg-pad-card-b)',
  },
)
const style = computed(() => ({
  '--lumina-panel-radius': toCssLength(props.radius),
  '--lumina-panel-padding': toCssLength(props.padding),
}))
</script>
<template>
  <component :is="as" class="lumina-panel" :class="`lumina-panel-${variant}`" :style="style">
    <slot />
  </component>
</template>
