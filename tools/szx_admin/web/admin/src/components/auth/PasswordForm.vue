<script setup lang="ts">
import Button from 'primevue/button'
import InputText from 'primevue/inputtext'
import { useI18n } from 'vue-i18n'
const { t } = useI18n()
const form = defineModel<{ oldPassword: string; password: string; confirmation: string }>({
  required: true,
})
defineProps<{
  busy: boolean
  error: string
  forced: boolean
  recommended: boolean
  minLength: number
  passwordHint: string
}>()
const emit = defineEmits<{ submit: []; skip: [] }>()
</script>
<template>
  <form class="form-stack" @submit.prevent="emit('submit')">
    <p class="alert" :class="forced ? 'warning' : 'info'">
      {{
        forced
          ? t('password.forced')
          : recommended
            ? t('password.recommended')
            : t('password.invalidatesSessions')
      }}
    </p>
    <p v-if="error" class="alert error" role="alert">{{ error }}</p>
    <label
      >{{ t('password.current')
      }}<InputText
        v-model="form.oldPassword"
        type="password"
        autocomplete="current-password"
        required
        :disabled="busy"
    /></label>
    <label
      >{{ t('password.new')
      }}<InputText
        v-model="form.password"
        type="password"
        autocomplete="new-password"
        :minlength="minLength"
        required
        aria-describedby="password-rule"
        :disabled="busy"
    /></label>
    <p id="password-rule" class="hint">{{ passwordHint }}{{ t('password.different') }}</p>
    <label
      >{{ t('password.confirmNew')
      }}<InputText
        v-model="form.confirmation"
        type="password"
        autocomplete="new-password"
        :minlength="minLength"
        required
        :disabled="busy"
    /></label>
    <div class="actions">
      <Button class="primary" :disabled="busy" type="submit">
        {{ busy ? t('password.submitting') : t('password.submit') }}</Button
      ><Button
        v-if="!forced"
        type="button"
        :disabled="busy"
        @click="emit('skip')"
        severity="secondary"
        outlined
      >
        {{ recommended ? t('password.skip') : t('password.back') }}
      </Button>
    </div>
  </form>
</template>
