<script setup lang="ts">
const form = defineModel<{ oldPassword: string; password: string; confirmation: string }>({
  required: true,
})
defineProps<{ busy: boolean; error: string; forced: boolean; recommended: boolean }>()
const emit = defineEmits<{ submit: []; skip: [] }>()
</script>
<template>
  <form class="form-stack" @submit.prevent="emit('submit')">
    <p class="alert" :class="forced ? 'warning' : 'info'">
      {{
        forced
          ? '首次登录必须修改密码，完成后才能进入控制台。'
          : recommended
            ? '建议修改临时密码，也可以跳过并稍后处理。'
            : '修改密码后，所有现有会话将失效，需要重新登录。'
      }}
    </p>
    <p v-if="error" class="alert error" role="alert">{{ error }}</p>
    <label
      >当前密码<input
        v-model="form.oldPassword"
        type="password"
        autocomplete="current-password"
        required
        :disabled="busy"
    /></label>
    <label
      >新密码<input
        v-model="form.password"
        type="password"
        autocomplete="new-password"
        minlength="12"
        required
        aria-describedby="password-rule"
        :disabled="busy"
    /></label>
    <p id="password-rule" class="hint">至少 12 个字符，包含字母和数字，且与当前密码不同。</p>
    <label
      >确认新密码<input
        v-model="form.confirmation"
        type="password"
        autocomplete="new-password"
        minlength="12"
        required
        :disabled="busy"
    /></label>
    <div class="actions">
      <button class="primary" :disabled="busy">{{ busy ? '正在修改…' : '修改密码' }}</button
      ><button v-if="!forced" type="button" :disabled="busy" @click="emit('skip')">
        {{ recommended ? '跳过，稍后修改' : '返回' }}
      </button>
    </div>
  </form>
</template>
