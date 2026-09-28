import { computed, onMounted, reactive, shallowRef } from 'vue'
import { adminApi } from '../api/admin'
import type { BehaviorEventRecord } from '../api/types'
import { redactDetails, toUtcSeconds } from '../domain'
import { usePagedQuery } from './usePagedQuery'
import { useI18n } from 'vue-i18n'

export function useBehaviorEventsViewModel() {
  const { t } = useI18n()
  const filters = reactive({ event_search: '', result: '', user_search: '', from: '', to: '' })
  let applied = { ...filters }
  const validationError = shallowRef('')
  const selected = shallowRef<BehaviorEventRecord | null>(null)
  const query = usePagedQuery<BehaviorEventRecord>((page, page_size) =>
    adminApi.behaviorEvents({
      page,
      page_size,
      ...Object.fromEntries(
        Object.entries({
          ...applied,
          from: toUtcSeconds(applied.from),
          to: toUtcSeconds(applied.to),
        }).filter(([, value]) => value !== ''),
      ),
    }),
  )
  const events = computed<BehaviorEventRecord[]>(() =>
    query.items.value.map((event) => ({
      ...event,
      details: redactDetails(event.details) as Record<string, unknown>,
    })),
  )
  function search() {
    validationError.value = ''
    if (!!filters.from !== !!filters.to) {
      validationError.value = t('audit.validationRange')
      return
    }
    if (filters.from && filters.to && new Date(filters.from) > new Date(filters.to)) {
      validationError.value = t('audit.validationOrder')
      return
    }
    applied = {
      ...filters,
      event_search: filters.event_search.trim(),
      user_search: filters.user_search.trim(),
    }
    void query.load(1)
  }
  function show(event: BehaviorEventRecord) {
    selected.value = { ...event, details: redactDetails(event.details) as Record<string, unknown> }
  }
  function close() {
    selected.value = null
  }
  function reset() {
    Object.assign(filters, { event_search: '', result: '', user_search: '', from: '', to: '' })
    search()
  }
  onMounted(() => query.load())
  return { filters, query, events, validationError, selected, search, reset, show, close }
}
