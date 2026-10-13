<script setup lang="ts">
import InputText from 'primevue/inputtext'

defineOptions({ inheritAttrs: false })
const model = defineModel<string | null>()
</script>
<template>
  <InputText class="lumina-smooth-edge" v-bind="$attrs" v-model="model" />
</template>
