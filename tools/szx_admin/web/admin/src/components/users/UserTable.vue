<script setup lang="ts">
import type { User } from '../../api/types'
import type { UserAction, UserRowView } from '../../composables/useUsersViewModel'
import { roleLabels, statusLabels, formatTime } from '../../domain'
import { useI18n } from 'vue-i18n'
const { t } = useI18n()
defineProps<{ users: UserRowView[]; busy: boolean; failed: boolean; canAssignRole: boolean }>()
const emit = defineEmits<{ action: [action: UserAction, user: User] }>()
</script>
<template>
  <div class="table-scroll" :aria-busy="busy">
    <table>
      <thead>
        <tr>
          <th>{{ t('common.user') }}</th>
          <th>{{ t('common.role') }}</th>
          <th>{{ t('common.status') }}</th>
          <th>{{ t('common.lastLogin') }}</th>
          <th class="action-column">{{ t('common.details') }}</th>
        </tr>
      </thead>
      <tbody>
        <tr v-for="user in users" :key="user.id">
          <td>
            <button class="user-link" @click="emit('action', 'details', user)">
              {{ user.display_name }}</button
            ><small class="identity">{{ user.login_identifier }}</small>
          </td>
          <td>
            <span class="role-badge">{{ t(roleLabels[user.role]) }}</span>
          </td>
          <td>
            <span class="status-badge" :class="user.status">{{
              t(statusLabels[user.status])
            }}</span>
          </td>
          <td class="time-cell">{{ formatTime(user.last_login_at) }}</td>
          <td>
            <div class="table-actions">
              <button :disabled="!user.canManage" @click="emit('action', 'name', user)">
                {{ t('users.edit') }}</button
              ><button
                v-if="canAssignRole"
                :disabled="!user.canAssignRole"
                @click="emit('action', 'role', user)"
              >
                {{ t('users.assignRole') }}</button
              ><button
                v-if="user.status === 'locked'"
                :disabled="!user.canManage"
                @click="emit('action', 'unlock', user)"
              >
                {{ t('users.unlock') }}</button
              ><button
                :class="user.status === 'disabled' ? '' : 'danger-text'"
                :disabled="!user.canChangeStatus"
                @click="emit('action', 'status', user)"
              >
                {{ user.status === 'disabled' ? t('users.enable') : t('users.disable') }}</button
              ><button :disabled="!user.canManage" @click="emit('action', 'reset', user)">
                {{ t('users.resetPassword') }}
              </button>
            </div>
          </td>
        </tr>
        <tr v-if="!users.length">
          <td colspan="5" class="empty">
            {{ busy ? t('users.loading') : failed ? t('users.failed') : t('users.empty') }}
          </td>
        </tr>
      </tbody>
    </table>
  </div>
</template>
