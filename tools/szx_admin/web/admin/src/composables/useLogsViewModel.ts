import { computed, reactive, shallowRef, watch } from 'vue'
import { adminApi } from '../api/admin'
import type { AuditLog } from '../api/types'
import { auditEventLabel, auditEventOptions, redactDetails, toUtcSeconds } from '../domain'
import { usePagedQuery } from './usePagedQuery'
import { useI18n } from 'vue-i18n'
import { useAuthStore } from '../stores/auth'
import { ApiError } from '../api/client'

export type AuditLogView = AuditLog & { eventLabel: string; actorUsername: string }

export function useLogsViewModel() {
  const { t } = useI18n()
  const auth = useAuthStore()
  const filters = reactive({ event_search: '', result: '', actor_search: '', from: '', to: '' })
  let applied = { ...filters }
  const validationError = shallowRef('')
  const selected = shallowRef<AuditLogView | null>(null)
  const query = usePagedQuery<AuditLog>((page, page_size) =>
    auth.can('audit.read')
      ? adminApi.auditLogs({
          page,
          page_size,
          ...Object.fromEntries(
            Object.entries({
              ...applied,
              from: toUtcSeconds(applied.from),
              to: toUtcSeconds(applied.to),
            }).filter(([, value]) => value !== ''),
          ),
        })
      : Promise.reject(new ApiError(2204)),
  )
  const logs = computed<AuditLogView[]>(() =>
    query.items.value.map((log) => ({
      ...log,
      eventLabel: auditEventLabel(log.event_name),
      actorUsername: log.actor_login_identifier || t('common.unknownUser'),
    })),
  )
  function search() {
    validationError.value = ''
    if (filters.from && filters.to && new Date(filters.from) > new Date(filters.to)) {
      validationError.value = t('audit.validationOrder')
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
      actorUsername: log.actor_login_identifier || t('common.unknownUser'),
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
  watch(
    () => auth.can('audit.read'),
    (allowed) => {
      if (allowed) void query.load()
      else {
        query.reset()
        selected.value = null
      }
    },
    { immediate: true },
  )
  return {
    filters,
    query,
    logs,
    eventOptions: computed(() => auditEventOptions()),
    validationError,
    selected,
    search,
    reset,
    show,
    close,
  }
}
