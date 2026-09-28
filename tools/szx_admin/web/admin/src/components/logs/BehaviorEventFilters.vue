<script setup lang="ts">
import { useI18n } from 'vue-i18n'
const { t } = useI18n()
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
      >{{ t('audit.eventName')
      }}<input
        v-model="filters.event_search"
        type="search"
        maxlength="128"
        :placeholder="t('audit.behaviorSearchPlaceholder')"
    /></label>
    <label
      >{{ t('audit.result')
      }}<select v-model="filters.result">
        <option value="">{{ t('audit.allResults') }}</option>
        <option value="success">{{ t('result.success') }}</option>
        <option value="failure">{{ t('result.failure') }}</option>
      </select></label
    >
    <label
      >{{ t('audit.searchUser')
      }}<input
        v-model="filters.user_search"
        type="search"
        maxlength="128"
        :placeholder="t('audit.searchUserPlaceholder')"
    /></label>
    <label
      >{{ t('audit.start') }}<input v-model="filters.from" type="datetime-local" step="1"
    /></label>
    <label>{{ t('audit.end') }}<input v-model="filters.to" type="datetime-local" step="1" /></label>
    <button class="primary" :disabled="busy">{{ t('audit.queryEvents') }}</button
    ><button type="button" :disabled="busy" @click="emit('reset')">{{ t('common.reset') }}</button>
  </form>
</template>
