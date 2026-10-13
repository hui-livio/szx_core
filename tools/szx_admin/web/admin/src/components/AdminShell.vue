<script setup lang="ts">
import Button from 'primevue/button'
import { useShellViewModel } from '../composables/useAuthViewModel'
import { useI18n } from 'vue-i18n'
import { useRoute } from 'vue-router'
import LanguageSwitcher from './LanguageSwitcher.vue'
const { links, logout, user, title, role, passwordLink, busy, error } = useShellViewModel()
const { t } = useI18n()
const route = useRoute()
</script>
<template>
  <div
    class="app-shell"
    :class="{ 'data-shell': route.path === '/users' || route.path === '/logs' }"
  >
    <aside class="sidebar">
      <div class="brand">
        <span class="brand-mark">S</span>
        <div>
          SZX <span class="brand-light">ADMIN</span
          ><small>{{ t('shell.identityManagement') }}</small>
        </div>
      </div>
      <div class="nav-label">{{ t('shell.adminConsole') }}</div>
      <nav :aria-label="t('shell.mainNavigation')">
        <RouterLink v-for="link in links" :key="link.to" :to="link.to"
          ><span aria-hidden="true">{{ link.symbol }}</span
          >{{ link.label }}</RouterLink
        >
      </nav>
      <div class="sidebar-bottom">
        <span class="system-mark" /> SZX · {{ t('shell.identityManagement') }}
      </div>
    </aside>
    <div class="workspace">
      <header class="topbar">
        <span class="breadcrumb"
          >{{ t('shell.console') }} <span>/</span> <strong>{{ title }}</strong></span
        >
        <div class="account">
          <LanguageSwitcher />
          <span class="avatar">{{ user?.display_name.slice(0, 1) }}</span>
          <div>
            {{ user?.display_name }}<small>{{ role }}</small>
          </div>
          <RouterLink v-if="passwordLink" to="/change-password">{{
            t('shell.password')
          }}</RouterLink
          ><Button
            class="text-button"
            :disabled="busy"
            @click="logout"
            type="button"
            severity="secondary"
            text
          >
            {{ busy ? t('shell.logoutBusy') : t('shell.logout') }}
          </Button>
        </div>
      </header>
      <main id="main" class="main">
        <p v-if="error" class="alert error" role="alert">{{ error }}</p>
        <slot />
      </main>
      <footer class="app-footer">
        SZX Admin <span>{{ t('shell.footer') }}</span>
      </footer>
    </div>
  </div>
</template>
