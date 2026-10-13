<script setup lang="ts">
import { LuminaButton as Button } from '../ui/lumina'
import { useHomeViewModel } from '../composables/useAuthViewModel'
import { formatTime } from '../domain'
import { useI18n } from 'vue-i18n'
const { user, role, capabilities, canManage, canAudit, busy, error, load } = useHomeViewModel()
const { t } = useI18n()
</script>
<template>
  <header class="page-heading">
    <div>
      <h1>{{ t('home.greeting', { name: user?.display_name }) }}</h1>
      <p>{{ t('home.subtitle') }}</p>
    </div>
    <span class="role-badge">{{ role }}</span>
  </header>
  <section class="panel">
    <h2>{{ t('home.accountOverview') }}</h2>
    <dl class="details-grid">
      <div>
        <dt>{{ t('common.loginIdentifier') }}</dt>
        <dd>{{ user?.login_identifier }}</dd>
      </div>
      <div>
        <dt>{{ t('home.recentLogin') }}</dt>
        <dd>{{ formatTime(user?.last_login_at ?? null) }}</dd>
      </div>
    </dl>
    <div class="actions">
      <RouterLink v-if="canManage" class="button primary" to="/users">{{
        t('home.enterUsers')
      }}</RouterLink>
      <RouterLink v-if="canAudit" class="button" to="/logs">{{ t('home.viewAudit') }}</RouterLink>
      <RouterLink class="button" to="/change-password">{{ t('shell.password') }}</RouterLink>
    </div>
  </section>
  <section class="panel">
    <div class="section-heading">
      <h2>{{ t('home.myPermissions') }}</h2>
      <Button :disabled="busy" @click="load" type="button">
        {{ busy ? t('common.loading') : t('home.refreshPermissions') }}
      </Button>
    </div>
    <p v-if="error" class="alert error" role="alert">{{ error }}</p>
    <div class="capability-list">
      <span v-for="item in capabilities" :key="item.code">✓ {{ item.label }}</span>
    </div>
    <p class="hint">{{ t('home.businessHint') }}</p>
  </section>
</template>
