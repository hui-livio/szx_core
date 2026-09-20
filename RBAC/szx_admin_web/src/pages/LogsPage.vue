<script setup lang="ts">
import { useLogsViewModel } from '../composables/useLogsViewModel'
import { formatTime } from '../domain'
import LogFilters from '../components/logs/LogFilters.vue'
import LogTable from '../components/logs/LogTable.vue'
import PaginationBar from '../components/PaginationBar.vue'
import ModalDialog from '../components/ModalDialog.vue'
const {
  filters,
  query,
  logs,
  eventOptions,
  validationError,
  selected,
  search,
  reset,
  show,
  close,
} = useLogsViewModel()
const { page, pageCount, total, busy, error, requestId, load } = query
</script>
<template>
  <header class="page-heading">
    <div>
      <p class="eyebrow">AUDIT TRAIL</p>
      <h1>审计日志</h1>
      <p>查询账号安全事件与用户行为，追溯每一次操作。</p>
    </div>
    <span class="role-badge">只读记录</span>
  </header>
  <section class="panel table-panel">
    <LogFilters
      v-model="filters"
      :busy="busy"
      :event-options="eventOptions"
      @search="search"
      @reset="reset"
    />
    <p v-if="validationError" class="alert error" role="alert">{{ validationError }}</p>
    <div class="section-heading table-heading">
      <h2>
        操作记录 <span class="count">{{ total }}</span>
      </h2>
      <button :disabled="busy" @click="load()">刷新日志</button>
    </div>
    <p v-if="error" class="alert error" role="alert">{{ error }}</p>
    <LogTable :logs="logs" :busy="busy" :failed="!!error" @details="show" /><PaginationBar
      :page="page"
      :page-count="pageCount"
      :total="total"
      :busy="busy"
      @change="load"
    />
  </section>
  <p v-if="requestId" class="request-id">请求 ID：{{ requestId }}</p>
  <ModalDialog v-if="selected" title="审计详情" @close="close"
    ><dl class="details-grid">
      <div>
        <dt>事件</dt>
        <dd>
          {{ selected.eventLabel }}
          <small class="identity"
            >事件代码：<span class="mono">{{ selected.event_name }}</span></small
          >
        </dd>
      </div>
      <div>
        <dt>事件 ID</dt>
        <dd class="mono">{{ selected.id }}</dd>
      </div>
      <div>
        <dt>时间</dt>
        <dd>{{ formatTime(selected.created_at) }}</dd>
      </div>
      <div>
        <dt>用户名</dt>
        <dd>{{ selected.actorUsername }}</dd>
      </div>
      <div>
        <dt>目标用户</dt>
        <dd class="mono">{{ selected.target_user_id || '—' }}</dd>
      </div>
      <div>
        <dt>请求 ID</dt>
        <dd class="mono">{{ selected.request_id }}</dd>
      </div>
    </dl>
    <h3>事件详情</h3>
    <pre class="json-details">{{ JSON.stringify(selected.details, null, 2) }}</pre>
    <div class="modal-actions"><button @click="close">关闭</button></div></ModalDialog
  >
</template>
