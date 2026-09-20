<script setup lang="ts">
const filters = defineModel<{
  event_name: string
  result: string
  actor_search: string
  from: string
  to: string
}>({ required: true })
defineProps<{ busy: boolean }>()
const emit = defineEmits<{ search: [] }>()
</script>
<template>
  <form class="filters log-filters" @submit.prevent="emit('search')">
    <label
      >事件名称<input v-model="filters.event_name" placeholder="例如 auth.login" maxlength="96"
    /></label>
    <label
      >结果<select v-model="filters.result">
        <option value="">全部结果</option>
        <option value="success">成功</option>
        <option value="failure">失败</option>
        <option value="denied">拒绝</option>
      </select></label
    >
    <label
      >搜索用户<input
        v-model="filters.actor_search"
        placeholder="搜索展示名、用户名或 UID"
        maxlength="128"
        type="search"
    /></label>
    <label>开始时间（本地）<input v-model="filters.from" type="datetime-local" step="1" /></label>
    <label>结束时间（本地）<input v-model="filters.to" type="datetime-local" step="1" /></label>
    <button class="primary" :disabled="busy">查询日志</button>
  </form>
</template>
