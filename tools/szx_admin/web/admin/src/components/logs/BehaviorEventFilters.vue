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
  user_search: string
  from: string
  to: string
}>({ required: true })
defineProps<{ busy: boolean }>()
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
])
</script>
<template>
  <form class="filters log-filters" @submit.prevent="emit('search')">
    <label
      >{{ t('audit.eventName')
      }}<InputText
        v-model="filters.event_search"
        type="search"
        maxlength="128"
        :placeholder="t('audit.behaviorSearchPlaceholder')"
    /></label>
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
        v-model="filters.user_search"
        type="search"
        maxlength="128"
        :placeholder="t('audit.searchUserPlaceholder')"
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
      t('audit.queryEvents')
    }}</Button
    ><Button type="button" :disabled="busy" @click="reset">{{ t('common.reset') }}</Button>
  </form>
</template>
