<script setup lang="ts">
import {
  LuminaButton as Button,
  LuminaTable as DataTable,
  LuminaColumn as Column,
} from '../../ui/lumina'
import type { BehaviorEventRecord } from '../../api/types'
import { formatTime, resultLabels } from '../../domain'
import { useI18n } from 'vue-i18n'
const { t } = useI18n()
defineProps<{ events: BehaviorEventRecord[]; busy: boolean; failed: boolean }>()
const emit = defineEmits<{ details: [event: BehaviorEventRecord] }>()
</script>
<template>
  <DataTable
    :value="events"
    data-key="id"
    class="admin-table"
    min-width="calc(var(--k) * 780px)"
    :pt="{ tableContainer: { class: 'table-scroll', 'aria-busy': busy } }"
  >
    <Column :header="`${t('common.event')} / ${t('common.requestId')}`">
      <template #body="{ data }: { data: BehaviorEventRecord }">
        <strong>{{ data.event_name }}</strong
        ><small class="identity"
          >{{ t('common.requestId') }}：<span class="mono">{{ data.request_id }}</span></small
        >
      </template>
    </Column>
    <Column :header="t('audit.result')">
      <template #body="{ data }: { data: BehaviorEventRecord }">
        <span class="status-badge" :class="data.result">{{ t(resultLabels[data.result]) }}</span>
      </template>
    </Column>
    <Column :header="t('common.user')">
      <template #body="{ data }: { data: BehaviorEventRecord }">{{
        data.login_identifier || t('common.unknownUser')
      }}</template>
    </Column>
    <Column :header="t('common.time')" body-class="time-cell">
      <template #body="{ data }: { data: BehaviorEventRecord }">{{
        formatTime(data.created_at)
      }}</template>
    </Column>
    <Column :header="t('common.details')">
      <template #body="{ data }: { data: BehaviorEventRecord }">
        <Button class="text-button" @click="emit('details', data)" type="button" variant="text">
          {{ t('common.details') }}
        </Button>
      </template>
    </Column>
    <template #empty>
      <div class="empty">
        {{ busy ? t('audit.loadingEvents') : failed ? t('audit.failed') : t('audit.emptyEvents') }}
      </div>
    </template>
  </DataTable>
</template>
