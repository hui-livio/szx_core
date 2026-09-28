<script setup lang="ts">
import type { CredentialIssue } from '../../api/types'
import { formatTime } from '../../domain'
import { useI18n } from 'vue-i18n'
const { t } = useI18n()
defineProps<{ issue: CredentialIssue; requestId: string }>()
const emit = defineEmits<{ close: [] }>()
</script>
<template>
  <div class="form-stack">
    <p class="alert warning">{{ t('users.credentialWarning') }}</p>
    <dl>
      <dt>{{ t('common.loginIdentifier') }}</dt>
      <dd>{{ issue.user.login_identifier }}</dd>
      <dt>{{ t('users.newPassword') }}</dt>
      <dd class="credential" data-testid="temporary-credential">
        {{ issue.temporary_credential }}
      </dd>
      <dt>{{ t('users.expiresAt') }}</dt>
      <dd>{{ formatTime(issue.temporary_credential_expires_at) }}</dd>
    </dl>
    <p class="request-id">{{ t('common.requestId') }}：{{ requestId }}</p>
    <button class="primary" @click="emit('close')">{{ t('users.acknowledged') }}</button>
  </div>
</template>
