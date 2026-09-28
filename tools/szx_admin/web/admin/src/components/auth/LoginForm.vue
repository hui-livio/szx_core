<script setup lang="ts">
import { useI18n } from 'vue-i18n'
const { t } = useI18n()
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
      >{{ t('login.identifier')
      }}<input
        v-model="form.identifier"
        name="identifier"
        autocomplete="username"
        :placeholder="t('login.identifierPlaceholder')"
        maxlength="128"
        required
        autofocus
        :disabled="busy"
    /></label>
    <label
      >{{ t('login.password')
      }}<input
        v-model="form.password"
        name="password"
        autocomplete="current-password"
        type="password"
        required
        :disabled="busy"
    /></label>
    <button class="primary login-submit" :disabled="busy">
      {{ busy ? t('login.submitting') : t('login.submit') }}<span aria-hidden="true">→</span>
    </button>
  </form>
</template>
