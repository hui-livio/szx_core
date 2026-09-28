<script setup lang="ts">
import { useI18n } from 'vue-i18n'
const { t } = useI18n()
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
      >{{ t('audit.eventName')
      }}<input
        v-model="filters.event_search"
        type="search"
        :placeholder="t('audit.eventSearchPlaceholder')"
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
      >{{ t('audit.result')
      }}<select v-model="filters.result">
        <option value="">{{ t('audit.allResults') }}</option>
        <option value="success">{{ t('result.success') }}</option>
        <option value="failure">{{ t('result.failure') }}</option>
        <option value="denied">{{ t('result.denied') }}</option>
      </select></label
    >
    <label
      >{{ t('audit.searchUser')
      }}<input
        v-model="filters.actor_search"
        :placeholder="t('audit.searchUserPlaceholder')"
        maxlength="128"
        type="search"
    /></label>
    <label
      >{{ t('audit.start') }}<input v-model="filters.from" type="datetime-local" step="1"
    /></label>
    <label>{{ t('audit.end') }}<input v-model="filters.to" type="datetime-local" step="1" /></label>
    <button class="primary" :disabled="busy">{{ t('audit.queryLogs') }}</button>
    <button type="button" :disabled="busy" @click="emit('reset')">{{ t('common.reset') }}</button>
  </form>
</template>
