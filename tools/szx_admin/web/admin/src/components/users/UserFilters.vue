<script setup lang="ts">
import type { Role, UserStatus } from '../../api/types'
import { roleLabels, statusLabels } from '../../domain'
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
      >搜索用户<input
        v-model="filters.search"
        placeholder="搜索登录标识或展示名"
        maxlength="128"
        type="search" /></label
    ><label
      >角色<select v-model="filters.role">
        <option value="">全部角色</option>
        <option v-for="role in roles" :key="role" :value="role">{{ roleLabels[role] }}</option>
      </select></label
    ><label
      >状态<select v-model="filters.status">
        <option value="">全部状态</option>
        <option v-for="(label, value) in statusLabels" :key="value" :value="value">
          {{ label }}
        </option>
      </select></label
    ><label
      >排序<select v-model="filters.sort">
        <option value="-created_at">最新创建</option>
        <option value="created_at">最早创建</option>
        <option value="display_name">展示名</option>
        <option value="-last_login_at">最近登录</option>
        <option value="role">角色</option>
        <option value="status">状态</option>
      </select></label
    ><button class="primary" :disabled="busy">查询</button>
  </form>
</template>
