<script setup lang="ts">
import InputText from 'primevue/inputtext'

defineOptions({ inheritAttrs: false })
const model = defineModel<string | null>()
</script>
<template>
  <InputText v-bind="$attrs" v-model="model" />
</template>
