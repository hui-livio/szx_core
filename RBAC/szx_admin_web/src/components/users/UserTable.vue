<script setup lang="ts">
import type { User } from '../../api/types'
import type { UserAction } from '../../composables/useUsersViewModel'
import { roleLabels, statusLabels, formatTime } from '../../domain'
defineProps<{ users: User[]; busy: boolean; failed: boolean; canAssignRole: boolean }>()
const emit = defineEmits<{ action: [action: UserAction, user: User] }>()
</script>
<template>
  <div class="table-scroll" :aria-busy="busy">
    <table>
      <thead>
        <tr>
          <th>用户</th>
          <th>角色</th>
          <th>状态</th>
          <th>最近登录</th>
          <th class="action-column">操作</th>
        </tr>
      </thead>
      <tbody>
        <tr v-for="user in users" :key="user.id">
          <td>
            <button class="user-link" @click="emit('action', 'details', user)">
              {{ user.display_name }}</button
            ><small class="identity">{{ user.uid ?? user.username }}</small>
          </td>
          <td>
            <span class="role-badge">{{ roleLabels[user.role] }}</span>
          </td>
          <td>
            <span class="status-badge" :class="user.status">{{ statusLabels[user.status] }}</span>
          </td>
          <td class="time-cell">{{ formatTime(user.last_login_at) }}</td>
          <td>
            <div class="table-actions">
              <button @click="emit('action', 'name', user)">编辑</button
              ><button v-if="canAssignRole" @click="emit('action', 'role', user)">分配角色</button
              ><button v-if="user.status === 'locked'" @click="emit('action', 'unlock', user)">
                解锁</button
              ><button
                :class="user.status === 'disabled' ? '' : 'danger-text'"
                @click="emit('action', 'status', user)"
              >
                {{ user.status === 'disabled' ? '启用' : '禁用' }}</button
              ><button @click="emit('action', 'reset', user)">重置密码</button>
            </div>
          </td>
        </tr>
        <tr v-if="!users.length">
          <td colspan="5" class="empty">
            {{
              busy
                ? '正在加载用户…'
                : failed
                  ? '加载失败，请重试。'
                  : '没有匹配的用户。调整筛选条件，或创建新用户。'
            }}
          </td>
        </tr>
      </tbody>
    </table>
  </div>
</template>
