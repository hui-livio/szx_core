<script setup lang="ts">
import type { BehaviorEventRecord } from '../../api/types'
import { formatTime, resultLabels } from '../../domain'
defineProps<{ events: BehaviorEventRecord[]; busy: boolean; failed: boolean }>()
const emit = defineEmits<{ details: [event: BehaviorEventRecord] }>()
</script>
<template>
  <div class="table-scroll" :aria-busy="busy">
    <table>
      <thead>
        <tr>
          <th>事件 / 请求 ID</th>
          <th>结果</th>
          <th>用户</th>
          <th>发生时间</th>
          <th>详情</th>
        </tr>
      </thead>
      <tbody>
        <tr v-for="event in events" :key="event.id">
          <td>
            <strong>{{ event.event_name }}</strong
            ><small class="identity"
              >请求 ID：<span class="mono">{{ event.request_id }}</span></small
            >
          </td>
          <td>
            <span class="status-badge" :class="event.result">{{ resultLabels[event.result] }}</span>
          </td>
          <td>{{ event.username || '未知用户' }}</td>
          <td class="time-cell">{{ formatTime(event.created_at) }}</td>
          <td><button class="text-button" @click="emit('details', event)">查看详情</button></td>
        </tr>
        <tr v-if="!events.length">
          <td colspan="5" class="empty">
            {{
              busy ? '正在加载行为事件…' : failed ? '加载失败，请重试。' : '没有匹配的行为事件。'
            }}
          </td>
        </tr>
      </tbody>
    </table>
  </div>
</template>
