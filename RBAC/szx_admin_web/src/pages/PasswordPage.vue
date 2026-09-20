<script setup lang="ts">
import PasswordForm from '../components/auth/PasswordForm.vue'
import { usePasswordViewModel } from '../composables/useAuthViewModel'
const { form, busy, error, forced, recommended, policy, submit, skip } = usePasswordViewModel()
</script>
<template>
  <header class="page-heading">
    <div>
      <p class="eyebrow">ACCOUNT SECURITY</p>
      <h1>修改密码</h1>
      <p>为你的账号设置新的登录密码。</p>
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
