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
  <header class="page-heading">
    <div>
      <p class="eyebrow">AUDIT TRAIL</p>
      <h1>{{ activeTab === 'audit' ? '安全审计' : '用户行为' }}</h1>
      <p>分别查询可信安全事实和客户端上传的非可信行为事件。</p>
    </div>
    <span class="role-badge">只读记录</span>
  </header>
  <nav class="log-tabs" aria-label="日志类型" role="tablist">
    <button
      role="tab"
      :aria-selected="activeTab === 'audit'"
      :class="{ active: activeTab === 'audit' }"
      @click="activeTab = 'audit'"
    >
      安全审计</button
    ><button
      role="tab"
      :aria-selected="activeTab === 'behavior'"
      :class="{ active: activeTab === 'behavior' }"
      @click="activeTab = 'behavior'"
    >
      用户行为
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
        操作记录 <span class="count">{{ total }}</span>
      </h2>
      <button :disabled="busy" @click="load()">刷新日志</button>
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
        用户行为 <span class="count">{{ behaviorTotal }}</span>
      </h2>
      <button :disabled="behaviorBusy" @click="loadBehavior()">刷新事件</button>
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
  <p v-if="activeTab === 'audit' && requestId" class="request-id">请求 ID：{{ requestId }}</p>
  <p v-if="activeTab === 'behavior' && behaviorRequestId" class="request-id">
    请求 ID：{{ behaviorRequestId }}
  </p>
  <ModalDialog v-if="auditSelected && activeTab === 'audit'" title="审计详情" @close="closeAudit"
    ><dl class="details-grid">
      <div>
        <dt>事件</dt>
        <dd>
          {{ auditSelected.eventLabel
          }}<small class="identity"
            >事件代码：<span class="mono">{{ auditSelected.event_name }}</span></small
          >
        </dd>
      </div>
      <div>
        <dt>事件 ID</dt>
        <dd class="mono">{{ auditSelected.id }}</dd>
      </div>
      <div>
        <dt>时间</dt>
        <dd>{{ formatTime(auditSelected.created_at) }}</dd>
      </div>
      <div>
        <dt>用户名</dt>
        <dd>{{ auditSelected.actorUsername }}</dd>
      </div>
      <div>
        <dt>目标用户</dt>
        <dd class="mono">{{ auditSelected.target_user_id || '—' }}</dd>
      </div>
      <div>
        <dt>请求 ID</dt>
        <dd class="mono">{{ auditSelected.request_id }}</dd>
      </div>
    </dl>
    <h3>事件详情</h3>
    <pre class="json-details">{{ JSON.stringify(auditSelected.details, null, 2) }}</pre>
    <div class="modal-actions"><button @click="closeAudit">关闭</button></div></ModalDialog
  >
  <ModalDialog
    v-if="behaviorSelected && activeTab === 'behavior'"
    title="用户行为详情"
    @close="closeBehavior"
    ><dl class="details-grid">
      <div>
        <dt>事件</dt>
        <dd class="mono">{{ behaviorSelected.event_name }}</dd>
      </div>
      <div>
        <dt>事件 ID</dt>
        <dd class="mono">{{ behaviorSelected.id }}</dd>
      </div>
      <div>
        <dt>时间</dt>
        <dd>{{ formatTime(behaviorSelected.created_at) }}</dd>
      </div>
      <div>
        <dt>用户名</dt>
        <dd>{{ behaviorSelected.username || '未知用户' }}</dd>
      </div>
      <div>
        <dt>用户 ID</dt>
        <dd class="mono">{{ behaviorSelected.user_id }}</dd>
      </div>
      <div>
        <dt>请求 ID</dt>
        <dd class="mono">{{ behaviorSelected.request_id }}</dd>
      </div>
    </dl>
    <h3>事件详情</h3>
    <pre class="json-details">{{ JSON.stringify(behaviorSelected.details, null, 2) }}</pre>
    <div class="modal-actions"><button @click="closeBehavior">关闭</button></div></ModalDialog
  >
</template>
