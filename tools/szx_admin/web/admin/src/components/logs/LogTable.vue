<script setup lang="ts">
import type { AuditLog } from '../../api/types'
import type { AuditLogView } from '../../composables/useLogsViewModel'
import { formatTime, resultLabels } from '../../domain'
defineProps<{ logs: AuditLogView[]; busy: boolean; failed: boolean }>()
const emit = defineEmits<{ details: [log: AuditLog] }>()
</script>
<template>
  <div class="table-scroll" :aria-busy="busy">
    <table>
      <thead>
        <tr>
          <th>事件 / 请求 ID</th>
          <th>结果</th>
          <th>登录标识</th>
          <th>发生时间</th>
          <th>详情</th>
        </tr>
      </thead>
      <tbody>
        <tr v-for="log in logs" :key="log.id">
          <td>
            <strong>{{ log.eventLabel }}</strong>
            <small class="identity"
              >事件代码：<span class="mono">{{ log.event_name }}</span></small
            >
            <small class="identity"
              >请求 ID：<span class="mono">{{ log.request_id }}</span></small
            >
          </td>
          <td>
            <span class="status-badge" :class="log.result">{{ resultLabels[log.result] }}</span>
          </td>
          <td>{{ log.actorUsername }}</td>
          <td class="time-cell">{{ formatTime(log.created_at) }}</td>
          <td><button class="text-button" @click="emit('details', log)">查看详情</button></td>
        </tr>
        <tr v-if="!logs.length">
          <td colspan="5" class="empty">
            {{
              busy
                ? '正在加载日志…'
                : failed
                  ? '加载失败，请重试。'
                  : '没有匹配的日志。请调整时间或筛选条件。'
            }}
          </td>
        </tr>
      </tbody>
    </table>
  </div>
</template>
