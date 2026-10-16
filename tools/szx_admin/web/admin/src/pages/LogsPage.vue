<script setup lang="ts">
import { LuminaPanel, LuminaButton as Button } from '../ui/lumina'
import { computed, ref, watch } from 'vue'
import { useLogsViewModel } from '../composables/useLogsViewModel'
import { useBehaviorEventsViewModel } from '../composables/useBehaviorEventsViewModel'
import { formatTime } from '../domain'
import LogFilters from '../components/logs/LogFilters.vue'
import LogTable from '../components/logs/LogTable.vue'
import BehaviorEventFilters from '../components/logs/BehaviorEventFilters.vue'
import BehaviorEventTable from '../components/logs/BehaviorEventTable.vue'
import PaginationBar from '../components/PaginationBar.vue'
import ModalDialog from '../components/ModalDialog.vue'
import { useI18n } from 'vue-i18n'
import { useAuthStore } from '../stores/auth'
const { t } = useI18n()
const auth = useAuthStore()
const canAudit = computed(() => auth.can('audit.read'))
const canBehavior = computed(() => auth.can('behavior.read'))

const audit = useLogsViewModel()
const behavior = useBehaviorEventsViewModel()
const activeTab = ref<'audit' | 'behavior'>(canAudit.value ? 'audit' : 'behavior')
watch([canAudit, canBehavior], () => {
  if (activeTab.value === 'audit' && !canAudit.value && canBehavior.value)
    activeTab.value = 'behavior'
  else if (activeTab.value === 'behavior' && !canBehavior.value && canAudit.value)
    activeTab.value = 'audit'
})
const {
  filters: auditFilters,
  logs,
  eventOptions,
  validationError: auditValidationError,
  selected: auditSelected,
  search: searchAudit,
  reset: resetAudit,
  show: showAudit,
  close: closeAudit,
} = audit
const {
  filters: behaviorFilters,
  events,
  eventOptions: behaviorEventOptions,
  validationError: behaviorValidationError,
  selected: behaviorSelected,
  search: searchBehavior,
  reset: resetBehavior,
  show: showBehavior,
  close: closeBehavior,
} = behavior
const { page, pageSize, pageCount, total, busy, error, requestId, load } = audit.query
const {
  page: behaviorPage,
  pageSize: behaviorPageSize,
  pageCount: behaviorPageCount,
  total: behaviorTotal,
  busy: behaviorBusy,
  error: behaviorError,
  requestId: behaviorRequestId,
  load: loadBehavior,
} = behavior.query
</script>

<template>
  <div class="data-page logs-page">
    <header class="page-heading">
      <div>
        <h1>{{ activeTab === 'audit' ? t('audit.title') : t('audit.behaviorTitle') }}</h1>
        <p>{{ t('audit.subtitle') }}</p>
      </div>
      <span class="role-badge">{{ t('audit.readOnly') }}</span>
    </header>
    <nav class="log-tabs" :aria-label="t('audit.tabs')" role="tablist">
      <Button
        v-if="canAudit"
        role="tab"
        :aria-selected="activeTab === 'audit'"
        :class="{ active: activeTab === 'audit' }"
        @click="activeTab = 'audit'"
        type="button"
      >
        {{ t('audit.auditTab') }}</Button
      ><Button
        v-if="canBehavior"
        role="tab"
        :aria-selected="activeTab === 'behavior'"
        :class="{ active: activeTab === 'behavior' }"
        @click="activeTab = 'behavior'"
        type="button"
      >
        {{ t('audit.behaviorTab') }}
      </Button>
    </nav>
    <LuminaPanel
      as="section"
      v-if="activeTab === 'audit' && canAudit"
      class="panel table-panel"
      radius="var(--lg-r-3xl)"
      padding="0"
    >
      <LogFilters
        v-model="auditFilters"
        :busy="busy"
        :event-options="eventOptions"
        @search="searchAudit"
        @reset="resetAudit"
      />
      <p v-if="auditValidationError" class="alert error" role="alert">{{ auditValidationError }}</p>
      <div class="section-heading table-heading">
        <h2>
          {{ t('audit.records') }} <span class="count">{{ total }}</span>
        </h2>
        <Button :disabled="busy" @click="load()" type="button">{{ t('audit.refreshLogs') }}</Button>
      </div>
      <p v-if="error" class="alert error" role="alert">{{ error }}</p>
      <LogTable :logs="logs" :busy="busy" :failed="!!error" @details="showAudit" /><PaginationBar
        :page="page"
        :page-size="pageSize"
        :page-count="pageCount"
        :total="total"
        :busy="busy"
        @change="load"
      />
    </LuminaPanel>
    <LuminaPanel
      as="section"
      v-else-if="activeTab === 'behavior' && canBehavior"
      class="panel table-panel"
      radius="var(--lg-r-3xl)"
      padding="0"
    >
      <BehaviorEventFilters
        v-model="behaviorFilters"
        :busy="behaviorBusy"
        :event-options="behaviorEventOptions"
        @search="searchBehavior"
        @reset="resetBehavior"
      />
      <p v-if="behaviorValidationError" class="alert error" role="alert">
        {{ behaviorValidationError }}
      </p>
      <div class="section-heading table-heading">
        <h2>
          {{ t('audit.events') }} <span class="count">{{ behaviorTotal }}</span>
        </h2>
        <Button :disabled="behaviorBusy" @click="loadBehavior()" type="button">
          {{ t('audit.refreshEvents') }}
        </Button>
      </div>
      <p v-if="behaviorError" class="alert error" role="alert">{{ behaviorError }}</p>
      <BehaviorEventTable
        :events="events"
        :busy="behaviorBusy"
        :failed="!!behaviorError"
        @details="showBehavior"
      /><PaginationBar
        :page="behaviorPage"
        :page-size="behaviorPageSize"
        :page-count="behaviorPageCount"
        :total="behaviorTotal"
        :busy="behaviorBusy"
        @change="loadBehavior"
      />
    </LuminaPanel>
    <p v-if="activeTab === 'audit' && requestId" class="request-id">
      {{ t('common.requestId') }}：{{ requestId }}
    </p>
    <p v-if="activeTab === 'behavior' && behaviorRequestId" class="request-id">
      {{ t('common.requestId') }}：{{ behaviorRequestId }}
    </p>
    <ModalDialog
      v-if="canAudit && auditSelected && activeTab === 'audit'"
      :title="t('audit.detailTitle')"
      @close="closeAudit"
      ><dl class="details-grid">
        <div>
          <dt>{{ t('common.event') }}</dt>
          <dd>
            {{ auditSelected.eventLabel
            }}<small class="identity"
              >{{ t('audit.eventCode') }}：<span class="mono">{{
                auditSelected.event_name
              }}</span></small
            >
          </dd>
        </div>
        <div>
          <dt>{{ t('common.eventId') }}</dt>
          <dd class="mono">{{ auditSelected.id }}</dd>
        </div>
        <div>
          <dt>{{ t('common.time') }}</dt>
          <dd>{{ formatTime(auditSelected.created_at) }}</dd>
        </div>
        <div>
          <dt>{{ t('common.loginIdentifier') }}</dt>
          <dd>{{ auditSelected.actorUsername }}</dd>
        </div>
        <div>
          <dt>{{ t('common.targetUser') }}</dt>
          <dd class="mono">{{ auditSelected.target_user_id || '—' }}</dd>
        </div>
        <div>
          <dt>{{ t('common.requestId') }}</dt>
          <dd class="mono">{{ auditSelected.request_id }}</dd>
        </div>
      </dl>
      <h3>{{ t('audit.details') }}</h3>
      <pre class="json-details">{{ JSON.stringify(auditSelected.details, null, 2) }}</pre>
      <div class="modal-actions">
        <Button @click="closeAudit" type="button">{{ t('common.close') }}</Button>
      </div></ModalDialog
    >
    <ModalDialog
      v-if="canBehavior && behaviorSelected && activeTab === 'behavior'"
      :title="t('audit.behaviorDetailTitle')"
      @close="closeBehavior"
      ><dl class="details-grid">
        <div>
          <dt>{{ t('common.event') }}</dt>
          <dd class="mono">{{ behaviorSelected.event_name }}</dd>
        </div>
        <div>
          <dt>{{ t('common.eventId') }}</dt>
          <dd class="mono">{{ behaviorSelected.id }}</dd>
        </div>
        <div>
          <dt>{{ t('common.time') }}</dt>
          <dd>{{ formatTime(behaviorSelected.created_at) }}</dd>
        </div>
        <div>
          <dt>{{ t('common.loginIdentifier') }}</dt>
          <dd>{{ behaviorSelected.login_identifier || t('common.unknownUser') }}</dd>
        </div>
        <div>
          <dt>{{ t('common.userId') }}</dt>
          <dd class="mono">{{ behaviorSelected.user_id }}</dd>
        </div>
        <div>
          <dt>{{ t('common.requestId') }}</dt>
          <dd class="mono">{{ behaviorSelected.request_id }}</dd>
        </div>
      </dl>
      <h3>{{ t('audit.details') }}</h3>
      <pre class="json-details">{{ JSON.stringify(behaviorSelected.details, null, 2) }}</pre>
      <div class="modal-actions">
        <Button @click="closeBehavior" type="button">{{ t('common.close') }}</Button>
      </div></ModalDialog
    >
  </div>
</template>
