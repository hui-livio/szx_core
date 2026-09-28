<script setup lang="ts">
import type { BehaviorEventRecord } from '../../api/types'
import { formatTime, resultLabels } from '../../domain'
import { useI18n } from 'vue-i18n'
const { t } = useI18n()
defineProps<{ events: BehaviorEventRecord[]; busy: boolean; failed: boolean }>()
const emit = defineEmits<{ details: [event: BehaviorEventRecord] }>()
</script>
<template>
  <div class="table-scroll" :aria-busy="busy">
    <table>
      <thead>
        <tr>
          <th>{{ t('common.event') }} / {{ t('common.requestId') }}</th>
          <th>{{ t('audit.result') }}</th>
          <th>{{ t('common.user') }}</th>
          <th>{{ t('audit.time') }}</th>
          <th>{{ t('common.details') }}</th>
        </tr>
      </thead>
      <tbody>
        <tr v-for="event in events" :key="event.id">
          <td>
            <strong>{{ event.event_name }}</strong
            ><small class="identity"
              >{{ t('common.requestId') }}：<span class="mono">{{ event.request_id }}</span></small
            >
          </td>
          <td>
            <span class="status-badge" :class="event.result">{{
              t(resultLabels[event.result])
            }}</span>
          </td>
          <td>{{ event.login_identifier || t('common.unknownUser') }}</td>
          <td class="time-cell">{{ formatTime(event.created_at) }}</td>
          <td>
            <button class="text-button" @click="emit('details', event)">
              {{ t('common.details') }}
            </button>
          </td>
        </tr>
        <tr v-if="!events.length">
          <td colspan="5" class="empty">
            {{
              busy ? t('audit.loadingEvents') : failed ? t('audit.failed') : t('audit.emptyEvents')
            }}
          </td>
        </tr>
      </tbody>
    </table>
  </div>
</template>
