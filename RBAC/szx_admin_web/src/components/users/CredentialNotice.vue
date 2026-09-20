<script setup lang="ts">
import type { CredentialIssue } from '../../api/types'
import { formatTime } from '../../domain'
defineProps<{ issue: CredentialIssue; requestId: string }>()
const emit = defineEmits<{ close: [] }>()
</script>
<template>
  <div class="form-stack">
    <p class="alert warning">临时密码仅展示本次。请通过安全渠道交付给用户，关闭后无法再次查看。</p>
    <dl>
      <dt>登录账号</dt>
      <dd>{{ issue.user.uid ?? issue.user.username }}</dd>
      <dt>临时密码</dt>
      <dd class="credential" data-testid="temporary-credential">
        {{ issue.temporary_credential }}
      </dd>
      <dt>有效期至</dt>
      <dd>{{ formatTime(issue.temporary_credential_expires_at) }}</dd>
    </dl>
    <p class="request-id">请求 ID：{{ requestId }}</p>
    <button class="primary" @click="emit('close')">已妥善记录，关闭</button>
  </div>
</template>
