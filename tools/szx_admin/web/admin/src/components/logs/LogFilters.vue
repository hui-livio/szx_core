<script setup lang="ts">
const filters = defineModel<{
  event_search: string
  result: string
  actor_search: string
  from: string
  to: string
}>({ required: true })
defineProps<{ busy: boolean; eventOptions: { code: string; label: string }[] }>()
const emit = defineEmits<{ search: []; reset: [] }>()
</script>
<template>
  <form class="filters log-filters" @submit.prevent="emit('search')">
    <label
      >事件名称<input
        v-model="filters.event_search"
        type="search"
        placeholder="搜索中文名称或事件代码，如登录、login"
        maxlength="128"
        list="audit-event-options"
        autocomplete="off"
    /></label>
    <datalist id="audit-event-options">
      <option
        v-for="event in eventOptions"
        :key="event.code"
        :value="event.label"
        :label="event.code"
      />
    </datalist>
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
        placeholder="搜索展示名或登录标识"
        maxlength="128"
        type="search"
    /></label>
    <label>开始时间（本地）<input v-model="filters.from" type="datetime-local" step="1" /></label>
    <label>结束时间（本地）<input v-model="filters.to" type="datetime-local" step="1" /></label>
    <button class="primary" :disabled="busy">查询日志</button>
    <button type="button" :disabled="busy" @click="emit('reset')">重置筛选</button>
  </form>
</template>
