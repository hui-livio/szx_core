<script setup lang="ts">
import type { AuditLog } from '../../api/types'
import type { AuditLogView } from '../../composables/useLogsViewModel'
import { formatTime, resultLabels } from '../../domain'
import { useI18n } from 'vue-i18n'
const { t } = useI18n()
defineProps<{ logs: AuditLogView[]; busy: boolean; failed: boolean }>()
const emit = defineEmits<{ details: [log: AuditLog] }>()
</script>
<template>
  <div class="table-scroll" :aria-busy="busy">
    <table>
      <thead>
        <tr>
          <th>{{ t('common.event') }} / {{ t('common.requestId') }}</th>
          <th>{{ t('audit.result') }}</th>
          <th>{{ t('common.loginIdentifier') }}</th>
          <th>{{ t('audit.time') }}</th>
          <th>{{ t('common.details') }}</th>
        </tr>
      </thead>
      <tbody>
        <tr v-for="log in logs" :key="log.id">
          <td>
            <strong>{{ log.eventLabel }}</strong>
            <small class="identity"
              >{{ t('audit.eventCode') }}：<span class="mono">{{ log.event_name }}</span></small
            >
            <small class="identity"
              >{{ t('common.requestId') }}：<span class="mono">{{ log.request_id }}</span></small
            >
          </td>
          <td>
            <span class="status-badge" :class="log.result">{{ t(resultLabels[log.result]) }}</span>
          </td>
          <td>{{ log.actorUsername }}</td>
          <td class="time-cell">{{ formatTime(log.created_at) }}</td>
          <td>
            <button class="text-button" @click="emit('details', log)">
              {{ t('common.details') }}
            </button>
          </td>
        </tr>
        <tr v-if="!logs.length">
          <td colspan="5" class="empty">
            {{ busy ? t('audit.loadingLogs') : failed ? t('audit.failed') : t('audit.emptyLogs') }}
          </td>
        </tr>
      </tbody>
    </table>
  </div>
</template>
