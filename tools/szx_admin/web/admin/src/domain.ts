import type { Role } from './api/types'
import { translate, i18n } from './i18n'

export const roleLabels: Record<Role, string> = {
  operator: 'roles.operator',
  engineer: 'roles.engineer',
  admin: 'roles.admin',
  super_admin: 'roles.super_admin',
}

export const statusLabels = {
  active: 'status.active',
  disabled: 'status.disabled',
  locked: 'status.locked',
}

export const resultLabels = {
  success: 'result.success',
  failure: 'result.failure',
  denied: 'result.denied',
}

const auditEventLabels: Readonly<Record<string, string>> = {
  'auth.login': 'events.auth_login',
  'auth.login.success': 'events.auth_login_success',
  'auth.login.failure': 'events.auth_login_failure',
  'auth.login.rate_limited': 'events.auth_login_rate_limited',
  'auth.account.locked': 'events.auth_account_locked',
  'auth.logout': 'events.auth_logout',
  'auth.refresh.success': 'events.auth_refresh_success',
  'auth.refresh.failure': 'events.auth_refresh_failure',
  'auth.refresh.replayed': 'events.auth_refresh_replayed',
  'auth.password.changed': 'events.auth_password_changed',
  'auth.password.change_failed': 'events.auth_password_change_failed',
  'user.created': 'events.user_created',
  'user.updated': 'events.user_updated',
  'user.status.changed': 'events.user_status_changed',
  'user.unlocked': 'events.user_unlocked',
  'user.role.changed': 'events.user_role_changed',
  'user.password.reset': 'events.user_password_reset',
}

export const roleLabel = (role: Role): string => translate(roleLabels[role])
export const statusLabel = (status: keyof typeof statusLabels): string =>
  translate(statusLabels[status])
export const resultLabel = (result: keyof typeof resultLabels): string =>
  translate(resultLabels[result])

export const auditEventLabel = (eventName: string): string =>
  Object.hasOwn(auditEventLabels, eventName)
    ? translate(auditEventLabels[eventName]!)
    : translate('events.unknown')

export const auditEventOptions = () =>
  Object.entries(auditEventLabels).map(([code, key]) => ({ code, label: translate(key) }))

export const isAdmin = (role?: Role) => role === 'admin' || role === 'super_admin'

export const manageableRoles = (role?: Role): Role[] =>
  role === 'super_admin'
    ? ['operator', 'engineer', 'admin', 'super_admin']
    : role === 'admin'
      ? ['operator', 'engineer']
      : []

export const formatTime = (value: number | null) =>
  value == null
    ? '—'
    : new Intl.DateTimeFormat(i18n.global.locale.value, {
        dateStyle: 'medium',
        timeStyle: 'medium',
        hour12: false,
      }).format(new Date(value))

// DatePicker emits local Date objects; never serialize these through toISOString().
export function formatDateTimeLocal(value: Date | null): string {
  if (!value || Number.isNaN(value.getTime())) return ''
  const pad = (part: number) => String(part).padStart(2, '0')
  return `${String(value.getFullYear()).padStart(4, '0')}-${pad(value.getMonth() + 1)}-${pad(value.getDate())}T${pad(value.getHours())}:${pad(value.getMinutes())}:${pad(value.getSeconds())}`
}

export function parseDateTimeLocal(value: string): Date | null {
  if (!/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}$/.test(value)) return null
  const date = new Date(value)
  return formatDateTimeLocal(date) === value ? date : null
}

export const toUtcSeconds = (value: string) =>
  value ? new Date(value).toISOString().replace(/\.\d{3}Z$/, 'Z') : ''

export async function hashPassword(password: string): Promise<string> {
  if (!globalThis.crypto?.subtle) throw new Error(translate('errors.secureLogin'))
  const bytes = await crypto.subtle.digest('SHA-256', new TextEncoder().encode(password))
  return Array.from(new Uint8Array(bytes), (byte) => byte.toString(16).padStart(2, '0')).join('')
}

export function passwordPolicy(role?: Role) {
  const simple = role === 'operator' || role === 'engineer'
  return {
    minLength: simple ? 6 : 8,
    requireLettersAndNumbers: !simple,
    hint: translate(simple ? 'password.policySimple' : 'password.policyStrong'),
  }
}

export function passwordProblem(
  password: string,
  oldPassword: string,
  confirmation: string,
  role?: Role,
): string {
  if (password !== confirmation) return translate('errors.passwordMismatch')
  if (password === oldPassword) return translate('errors.passwordSame')
  const policy = passwordPolicy(role)
  if (
    [...password].length < policy.minLength ||
    (policy.requireLettersAndNumbers && (!/\p{L}/u.test(password) || !/\p{N}/u.test(password)))
  )
    return translate('errors.passwordPolicy', { hint: policy.hint })
  return ''
}

export function redactDetails(value: unknown, depth = 0): unknown {
  if (depth >= 32) return null
  if (Array.isArray(value)) return value.map((item) => redactDetails(item, depth + 1))
  if (value && typeof value === 'object') {
    return Object.fromEntries(
      Object.entries(value)
        .filter(([key]) => {
          const normalized = key.toLowerCase().replace(/[^a-z0-9]/g, '')
          return (
            !/(password|token|secret|privatekey|apikey|credential|machineid|hardwareid|fingerprint|stack|traceback)/.test(
              normalized,
            ) && !['authorization', 'cookie', 'setcookie'].includes(normalized)
          )
        })
        .map(([key, item]) => [key, redactDetails(item, depth + 1)]),
    )
  }
  return value
}
