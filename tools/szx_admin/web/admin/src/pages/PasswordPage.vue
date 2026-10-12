<script setup lang="ts">
import PasswordForm from '../components/auth/PasswordForm.vue'
import { usePasswordViewModel } from '../composables/useAuthViewModel'
import { useI18n } from 'vue-i18n'
const { form, busy, error, forced, recommended, policy, submit, skip } = usePasswordViewModel()
const { t } = useI18n()
</script>
<template>
  <header class="page-heading">
    <div>
      <h1>{{ t('password.title') }}</h1>
      <p>{{ t('password.subtitle') }}</p>
    </div>
  </header>
  <section class="panel narrow">
    <PasswordForm
      v-model="form"
      :busy="busy"
      :error="error"
      :forced="forced"
      :recommended="recommended"
      :min-length="policy.minLength"
      :password-hint="policy.hint"
      @submit="submit"
      @skip="skip"
    />
  </section>
</template>
