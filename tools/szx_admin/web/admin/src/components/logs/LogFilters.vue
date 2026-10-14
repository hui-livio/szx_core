<script setup lang="ts">
import {
  LuminaButton as Button,
  LuminaInput as InputText,
  LuminaSelect as Select,
} from '../../ui/lumina'
import LocalDateTimePicker from '../LocalDateTimePicker.vue'
import { computed, shallowRef } from 'vue'
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
const dateInputKey = shallowRef(0)
function reset() {
  // Reset also discards incomplete manual input that has no committed model value.
  dateInputKey.value++
  emit('reset')
}
const resultOptions = computed(() => [
  { value: '', label: t('audit.allResults') },
  { value: 'success', label: t('result.success') },
  { value: 'failure', label: t('result.failure') },
  { value: 'denied', label: t('result.denied') },
])
</script>
<template>
  <form class="filters log-filters" @submit.prevent="emit('search')">
    <label>
      {{ t('audit.eventName') }}
      <Select
        v-model="filters.event_search"
        editable
        :options="eventOptions"
        option-label="label"
        option-value="code"
        :empty-message="t('common.noResults')"
        :placeholder="t('audit.eventSearchPlaceholder')"
        :aria-label="t('audit.eventName')"
        :pt="{
          label: { maxlength: 128, spellcheck: false },
          listContainer: { class: 'lumina-scrollbar' },
        }"
      >
        <template #option="{ option }">
          <div class="lumina-select-option-content">
            <span class="lumina-select-option-label">{{ option.label }}</span>
            <span class="lumina-select-option-detail">{{ option.code }}</span>
          </div>
        </template>
      </Select>
    </label>
    <label
      >{{ t('audit.result')
      }}<Select
        v-model="filters.result"
        :placeholder="t('audit.allResults')"
        :options="resultOptions"
        option-label="label"
        option-value="value"
        :aria-label="t('audit.result')"
    /></label>
    <label
      >{{ t('audit.searchUser')
      }}<InputText
        v-model="filters.actor_search"
        :placeholder="t('audit.searchUserPlaceholder')"
        maxlength="128"
        type="search"
    /></label>
    <label
      >{{ t('audit.start')
      }}<LocalDateTimePicker
        :key="`from-${dateInputKey}`"
        v-model="filters.from"
        :label="t('audit.start')"
    /></label>
    <label
      >{{ t('audit.end')
      }}<LocalDateTimePicker
        :key="`to-${dateInputKey}`"
        v-model="filters.to"
        :label="t('audit.end')"
    /></label>
    <Button class="primary" :disabled="busy" type="submit" variant="primary">{{
      t('audit.queryLogs')
    }}</Button>
    <Button type="button" :disabled="busy" @click="reset">{{ t('common.reset') }}</Button>
  </form>
</template>
