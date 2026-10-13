<script setup lang="ts">
import Button from 'primevue/button'
import DataTable from 'primevue/datatable'
import Column from 'primevue/column'
import type { AuditLog } from '../../api/types'
import type { AuditLogView } from '../../composables/useLogsViewModel'
import { formatTime, resultLabels } from '../../domain'
import { useI18n } from 'vue-i18n'
const { t } = useI18n()
defineProps<{ logs: AuditLogView[]; busy: boolean; failed: boolean }>()
const emit = defineEmits<{ details: [log: AuditLog] }>()
</script>
<template>
  <DataTable
    :value="logs"
    data-key="id"
    class="admin-table"
    :pt="{ tableContainer: { class: 'table-scroll', 'aria-busy': busy } }"
  >
    <Column :header="`${t('common.event')} / ${t('common.requestId')}`">
      <template #body="{ data }: { data: AuditLogView }">
        <strong>{{ data.eventLabel }}</strong>
        <small class="identity"
          >{{ t('audit.eventCode') }}：<span class="mono">{{ data.event_name }}</span></small
        >
        <small class="identity"
          >{{ t('common.requestId') }}：<span class="mono">{{ data.request_id }}</span></small
        >
      </template>
    </Column>
    <Column :header="t('audit.result')">
      <template #body="{ data }: { data: AuditLogView }">
        <span class="status-badge" :class="data.result">{{ t(resultLabels[data.result]) }}</span>
      </template>
    </Column>
    <Column :header="t('common.loginIdentifier')">
      <template #body="{ data }: { data: AuditLogView }">{{ data.actorUsername }}</template>
    </Column>
    <Column :header="t('common.time')" body-class="time-cell">
      <template #body="{ data }: { data: AuditLogView }">{{
        formatTime(data.created_at)
      }}</template>
    </Column>
    <Column :header="t('common.details')">
      <template #body="{ data }: { data: AuditLogView }">
        <Button
          class="text-button"
          @click="emit('details', data)"
          type="button"
          severity="secondary"
          text
        >
          {{ t('common.details') }}
        </Button>
      </template>
    </Column>
    <template #empty>
      <div class="empty">
        {{ busy ? t('audit.loadingLogs') : failed ? t('audit.failed') : t('audit.emptyLogs') }}
      </div>
    </template>
  </DataTable>
</template>
