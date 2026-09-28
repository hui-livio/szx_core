<script setup lang="ts">
const form = defineModel<{
  identifier: string
  password: string
}>({ required: true })
defineProps<{ busy: boolean; error: string; notice: string }>()
const emit = defineEmits<{ submit: [] }>()
</script>
<template>
  <form class="form-stack" @submit.prevent="emit('submit')">
    <p v-if="notice" class="alert success" role="status">{{ notice }}</p>
    <p v-if="error" class="alert error" role="alert">{{ error }}</p>
    <label
      >登录标识<input
        v-model="form.identifier"
        name="identifier"
        autocomplete="username"
        placeholder="请输入登录标识"
        maxlength="128"
        required
        autofocus
        :disabled="busy"
    /></label>
    <label
      >密码<input
        v-model="form.password"
        name="password"
        autocomplete="current-password"
        type="password"
        required
        :disabled="busy"
    /></label>
    <button class="primary login-submit" :disabled="busy">
      {{ busy ? '正在登录…' : '登录控制台' }}<span aria-hidden="true">→</span>
    </button>
  </form>
</template>
