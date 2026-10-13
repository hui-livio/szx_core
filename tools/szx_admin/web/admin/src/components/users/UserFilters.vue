<script setup lang="ts">
import {
  LuminaButton as Button,
  LuminaInput as InputText,
  LuminaSelect as Select,
} from '../../ui/lumina'
import { computed } from 'vue'
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
const props = defineProps<{ roles: Role[]; busy: boolean }>()
const emit = defineEmits<{ search: [] }>()
const roleOptions = computed(() => [
  { value: '', label: t('users.allRoles') },
  ...props.roles.map((value) => ({ value, label: t(roleLabels[value]) })),
])
const statusOptions = computed(() => [
  { value: '', label: t('users.allStatuses') },
  ...Object.entries(statusLabels).map(([value, label]) => ({ value, label: t(label) })),
])
const sortOptions = computed(() => [
  { value: '-created_at', label: t('users.newest') },
  { value: 'created_at', label: t('users.oldest') },
  { value: 'display_name', label: t('users.displayName') },
  { value: '-last_login_at', label: t('users.newestLogin') },
  { value: 'role', label: t('common.role') },
  { value: 'status', label: t('common.status') },
])
</script>
<template>
  <form class="filters" @submit.prevent="emit('search')">
    <label class="search-field"
      >{{ t('users.search')
      }}<InputText
        v-model="filters.search"
        :placeholder="t('users.searchPlaceholder')"
        maxlength="128"
        type="search" /></label
    ><label
      >{{ t('common.role')
      }}<Select
        v-model="filters.role"
        :placeholder="t('users.allRoles')"
        :options="roleOptions"
        option-label="label"
        option-value="value"
        :aria-label="t('common.role')" /></label
    ><label
      >{{ t('common.status')
      }}<Select
        v-model="filters.status"
        :placeholder="t('users.allStatuses')"
        :options="statusOptions"
        option-label="label"
        option-value="value"
        :aria-label="t('common.status')" /></label
    ><label
      >{{ t('users.sort')
      }}<Select
        v-model="filters.sort"
        :options="sortOptions"
        option-label="label"
        option-value="value"
        :aria-label="t('users.sort')" /></label
    ><Button class="primary" :disabled="busy" type="submit" variant="primary">{{
      t('common.search')
    }}</Button>
  </form>
</template>
