<script setup lang="ts">
import { ref } from 'vue'
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
const { t } = useI18n()

const audit = useLogsViewModel()
const behavior = useBehaviorEventsViewModel()
const activeTab = ref<'audit' | 'behavior'>('audit')
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
  validationError: behaviorValidationError,
  selected: behaviorSelected,
  search: searchBehavior,
  reset: resetBehavior,
  show: showBehavior,
  close: closeBehavior,
} = behavior
const { page, pageCount, total, busy, error, requestId, load } = audit.query
const {
  page: behaviorPage,
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
      <button
        role="tab"
        :aria-selected="activeTab === 'audit'"
        :class="{ active: activeTab === 'audit' }"
        @click="activeTab = 'audit'"
      >
        {{ t('audit.auditTab') }}</button
      ><button
        role="tab"
        :aria-selected="activeTab === 'behavior'"
        :class="{ active: activeTab === 'behavior' }"
        @click="activeTab = 'behavior'"
      >
        {{ t('audit.behaviorTab') }}
      </button>
    </nav>
    <section v-if="activeTab === 'audit'" class="panel table-panel">
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
        <button :disabled="busy" @click="load()">{{ t('audit.refreshLogs') }}</button>
      </div>
      <p v-if="error" class="alert error" role="alert">{{ error }}</p>
      <LogTable :logs="logs" :busy="busy" :failed="!!error" @details="showAudit" /><PaginationBar
        :page="page"
        :page-count="pageCount"
        :total="total"
        :busy="busy"
        @change="load"
      />
    </section>
    <section v-else class="panel table-panel">
      <BehaviorEventFilters
        v-model="behaviorFilters"
        :busy="behaviorBusy"
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
        <button :disabled="behaviorBusy" @click="loadBehavior()">
          {{ t('audit.refreshEvents') }}
        </button>
      </div>
      <p v-if="behaviorError" class="alert error" role="alert">{{ behaviorError }}</p>
      <BehaviorEventTable
        :events="events"
        :busy="behaviorBusy"
        :failed="!!behaviorError"
        @details="showBehavior"
      /><PaginationBar
        :page="behaviorPage"
        :page-count="behaviorPageCount"
        :total="behaviorTotal"
        :busy="behaviorBusy"
        @change="loadBehavior"
      />
    </section>
    <p v-if="activeTab === 'audit' && requestId" class="request-id">
      {{ t('common.requestId') }}：{{ requestId }}
    </p>
    <p v-if="activeTab === 'behavior' && behaviorRequestId" class="request-id">
      {{ t('common.requestId') }}：{{ behaviorRequestId }}
    </p>
    <ModalDialog
      v-if="auditSelected && activeTab === 'audit'"
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
        <button @click="closeAudit">{{ t('common.close') }}</button>
      </div></ModalDialog
    >
    <ModalDialog
      v-if="behaviorSelected && activeTab === 'behavior'"
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
        <button @click="closeBehavior">{{ t('common.close') }}</button>
      </div></ModalDialog
    >
  </div>
</template>
