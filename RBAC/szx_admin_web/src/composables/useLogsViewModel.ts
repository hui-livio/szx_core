import { computed, onMounted, reactive, shallowRef } from 'vue'
import { adminApi } from '../api/admin'
import type { AuditLog } from '../api/types'
import { auditEventLabel, auditEventOptions, redactDetails, toUtcSeconds } from '../domain'
import { usePagedQuery } from './usePagedQuery'

export type AuditLogView = AuditLog & { eventLabel: string; actorUsername: string }

export function useLogsViewModel() {
  const filters = reactive({ event_search: '', result: '', actor_search: '', from: '', to: '' })
  let applied = { ...filters }
  const validationError = shallowRef('')
  const selected = shallowRef<AuditLogView | null>(null)
  const query = usePagedQuery<AuditLog>((page, page_size) =>
    adminApi.logs({
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
  const logs = computed<AuditLogView[]>(() =>
    query.items.value.map((log) => ({
      ...log,
      eventLabel: auditEventLabel(log.event_name),
      actorUsername: log.actor_username || '未知用户',
    })),
  )
  function search() {
    validationError.value = ''
    if (filters.from && filters.to && new Date(filters.from) > new Date(filters.to)) {
      validationError.value = '开始时间不能晚于结束时间。'
      return
    }
    applied = {
      ...filters,
      event_search: filters.event_search.trim(),
      actor_search: filters.actor_search.trim(),
    }
    void query.load(1)
  }
  function show(log: AuditLog) {
    selected.value = {
      ...log,
      eventLabel: auditEventLabel(log.event_name),
      actorUsername: log.actor_username || '未知用户',
      details: redactDetails(log.details) as Record<string, unknown>,
    }
  }
  function close() {
    selected.value = null
  }
  function reset() {
    Object.assign(filters, { event_search: '', result: '', actor_search: '', from: '', to: '' })
    search()
  }
  onMounted(() => query.load())
  return {
    filters,
    query,
    logs,
    eventOptions: auditEventOptions,
    validationError,
    selected,
    search,
    reset,
    show,
    close,
  }
}
