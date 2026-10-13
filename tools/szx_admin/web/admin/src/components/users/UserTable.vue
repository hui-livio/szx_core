<script setup lang="ts">
import {
  LuminaButton as Button,
  LuminaTable as DataTable,
  LuminaColumn as Column,
} from '../../ui/lumina'
import type { User } from '../../api/types'
import type { UserAction, UserRowView } from '../../composables/useUsersViewModel'
import { roleLabels, statusLabels, formatTime } from '../../domain'
import { useI18n } from 'vue-i18n'
const { t } = useI18n()
defineProps<{ users: UserRowView[]; busy: boolean; failed: boolean; canAssignRole: boolean }>()
const emit = defineEmits<{ action: [action: UserAction, user: User] }>()
</script>
<template>
  <DataTable
    :value="users"
    data-key="id"
    class="admin-table"
    min-width="calc(var(--k) * 780px)"
    :pt="{ tableContainer: { class: 'table-scroll', 'aria-busy': busy } }"
  >
    <Column :header="t('common.user')">
      <template #body="{ data }: { data: UserRowView }">
        <Button
          class="user-link"
          @click="emit('action', 'details', data)"
          type="button"
          variant="text"
        >
          {{ data.display_name }}</Button
        ><small class="identity">{{ data.login_identifier }}</small>
      </template>
    </Column>
    <Column :header="t('common.role')">
      <template #body="{ data }: { data: UserRowView }">
        <span class="role-badge">{{ t(roleLabels[data.role]) }}</span>
      </template>
    </Column>
    <Column :header="t('common.status')">
      <template #body="{ data }: { data: UserRowView }">
        <span class="status-badge" :class="data.status">{{ t(statusLabels[data.status]) }}</span>
      </template>
    </Column>
    <Column :header="t('common.lastLogin')" body-class="time-cell">
      <template #body="{ data }: { data: UserRowView }">{{
        formatTime(data.last_login_at)
      }}</template>
    </Column>
    <Column :header="t('common.details')" header-class="action-column">
      <template #body="{ data }: { data: UserRowView }">
        <div class="table-actions">
          <Button
            :disabled="!data.canManage"
            @click="emit('action', 'name', data)"
            type="button"
            variant="text"
          >
            {{ t('users.edit') }}</Button
          ><Button
            v-if="canAssignRole"
            :disabled="!data.canAssignRole"
            @click="emit('action', 'role', data)"
            type="button"
            variant="text"
          >
            {{ t('users.assignRole') }}</Button
          ><Button
            v-if="data.status === 'locked'"
            :disabled="!data.canManage"
            @click="emit('action', 'unlock', data)"
            type="button"
            variant="text"
          >
            {{ t('users.unlock') }}</Button
          ><Button
            :class="data.status === 'disabled' ? '' : 'danger-text'"
            :disabled="!data.canChangeStatus"
            @click="emit('action', 'status', data)"
            type="button"
            variant="text"
          >
            {{ data.status === 'disabled' ? t('users.enable') : t('users.disable') }}</Button
          ><Button
            :disabled="!data.canManage"
            @click="emit('action', 'reset', data)"
            type="button"
            variant="text"
          >
            {{ t('users.resetPassword') }}
          </Button>
        </div>
      </template>
    </Column>
    <template #empty>
      <div class="empty">
        {{ busy ? t('users.loading') : failed ? t('users.failed') : t('users.empty') }}
      </div>
    </template>
  </DataTable>
</template>
