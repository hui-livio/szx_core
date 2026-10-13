<script setup lang="ts">
import Button from 'primevue/button'
import type { ButtonSlots } from 'primevue/button'

defineOptions({ inheritAttrs: false })
withDefaults(
  defineProps<{
    variant?: 'primary' | 'secondary' | 'text' | 'danger'
    type?: 'button' | 'submit' | 'reset'
    label?: string
    disabled?: boolean
    loading?: boolean
  }>(),
  { variant: 'secondary', type: 'button' },
)
const emit = defineEmits<{ click: [event: MouseEvent] }>()
defineSlots<ButtonSlots>()
</script>
<template>
  <Button
    v-bind="$attrs"
    :type="type"
    :label="label"
    :disabled="disabled"
    :loading="loading"
    :class="{ primary: variant === 'primary' }"
    :severity="variant === 'primary' ? undefined : variant === 'danger' ? 'danger' : 'secondary'"
    :outlined="variant === 'secondary' || variant === 'danger'"
    :text="variant === 'text'"
    @click="emit('click', $event)"
  >
    <template v-for="(_, name) in $slots" #[name]="slotProps">
      <slot :name="name as keyof ButtonSlots" v-bind="slotProps ?? {}" />
    </template>
  </Button>
</template>
