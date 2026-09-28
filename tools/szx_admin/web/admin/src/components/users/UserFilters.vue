<script setup lang="ts">
import type { Role, UserStatus } from '../../api/types'
import { roleLabels, statusLabels } from '../../domain'
import { useI18n } from 'vue-i18n'
const { t } = useI18n()
const filters = defineModel<{
  search: string
  role: Role | ''
  status: UserStatus | ''
  sort: string
}>({ required: true })
defineProps<{ roles: Role[]; busy: boolean }>()
const emit = defineEmits<{ search: [] }>()
</script>
<template>
  <form class="filters" @submit.prevent="emit('search')">
    <label class="search-field"
      >{{ t('users.search')
      }}<input
        v-model="filters.search"
        :placeholder="t('users.searchPlaceholder')"
        maxlength="128"
        type="search" /></label
    ><label
      >{{ t('common.role')
      }}<select v-model="filters.role">
        <option value="">{{ t('users.allRoles') }}</option>
        <option v-for="role in roles" :key="role" :value="role">{{ t(roleLabels[role]) }}</option>
      </select></label
    ><label
      >{{ t('common.status')
      }}<select v-model="filters.status">
        <option value="">{{ t('users.allStatuses') }}</option>
        <option v-for="(label, value) in statusLabels" :key="value" :value="value">
          {{ t(label) }}
        </option>
      </select></label
    ><label
      >{{ t('users.sort')
      }}<select v-model="filters.sort">
        <option value="-created_at">{{ t('users.newest') }}</option>
        <option value="created_at">{{ t('users.oldest') }}</option>
        <option value="display_name">{{ t('users.displayName') }}</option>
        <option value="-last_login_at">{{ t('users.newestLogin') }}</option>
        <option value="role">{{ t('common.role') }}</option>
        <option value="status">{{ t('common.status') }}</option>
      </select></label
    ><button class="primary" :disabled="busy">{{ t('common.search') }}</button>
  </form>
</template>
