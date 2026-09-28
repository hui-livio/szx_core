<script setup lang="ts">
const filters = defineModel<{
  event_search: string
  result: string
  user_search: string
  from: string
  to: string
}>({ required: true })
defineProps<{ busy: boolean }>()
const emit = defineEmits<{ search: []; reset: [] }>()
</script>
<template>
  <form class="filters log-filters" @submit.prevent="emit('search')">
    <label
      >事件名称<input
        v-model="filters.event_search"
        type="search"
        maxlength="128"
        placeholder="搜索客户端事件代码"
    /></label>
    <label
      >结果<select v-model="filters.result">
        <option value="">全部结果</option>
        <option value="success">成功</option>
        <option value="failure">失败</option>
      </select></label
    >
    <label
      >搜索用户<input
        v-model="filters.user_search"
        type="search"
        maxlength="128"
        placeholder="搜索展示名或登录标识"
    /></label>
    <label>开始时间（本地）<input v-model="filters.from" type="datetime-local" step="1" /></label>
    <label>结束时间（本地）<input v-model="filters.to" type="datetime-local" step="1" /></label>
    <button class="primary" :disabled="busy">查询事件</button
    ><button type="button" :disabled="busy" @click="emit('reset')">重置筛选</button>
  </form>
</template>
