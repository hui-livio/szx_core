<script setup lang="ts">
import LoginForm from '../components/auth/LoginForm.vue'
import { useLoginViewModel } from '../composables/useAuthViewModel'
import { useI18n } from 'vue-i18n'
import LanguageSwitcher from '../components/LanguageSwitcher.vue'
const { form, busy, error, notice, submit } = useLoginViewModel()
const { t } = useI18n()
</script>
<template>
  <main class="login-page">
    <div class="login-language"><LanguageSwitcher /></div>
    <section class="login-story">
      <div class="brand">
        <span class="brand-mark">S</span>SZX <span class="brand-light">ADMIN</span>
      </div>
      <div class="login-copy">
        <p class="eyebrow">{{ t('login.eyebrow') }}</p>
        <h1>
          <template v-for="(line, index) in t('login.heroTitle').split('\n')" :key="line">
            {{ line }}<br v-if="index < t('login.heroTitle').split('\n').length - 1" />
          </template>
        </h1>
        <p>
          <template v-for="(line, index) in t('login.heroSubtitle').split('\n')" :key="line">
            {{ line }}<br v-if="index < t('login.heroSubtitle').split('\n').length - 1" />
          </template>
        </p>
        <div class="role-path">
          <span>{{ t('roles.operator') }}</span
          ><i /><span>{{ t('roles.engineer') }}</span
          ><i /><span>{{ t('roles.admin') }}</span>
        </div>
      </div>
      <small>SZX · {{ t('shell.identityManagement') }}</small>
    </section>
    <section class="login-panel">
      <div class="login-card">
        <p class="eyebrow">{{ t('login.console') }}</p>
        <h2>{{ t('login.welcome') }}</h2>
        <p class="subtitle">{{ t('login.continue') }}</p>
        <LoginForm v-model="form" :busy="busy" :error="error" :notice="notice" @submit="submit" />
        <p class="login-help">{{ t('login.help') }}</p>
      </div>
      <small>{{ t('login.authorizedOnly') }}</small>
    </section>
  </main>
</template>
