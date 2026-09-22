import type { Role } from './api/types'

export const roleLabels: Record<Role, string> = {
  operator: '操作员',
  engineer: '工程师',
  admin: '管理员',
  super_admin: '超级管理员',
}

export const statusLabels = { active: '已启用', disabled: '已禁用', locked: '已锁定' }

export const resultLabels = { success: '成功', failure: '失败', denied: '拒绝' }

const auditEventLabels: Readonly<Record<string, string>> = {
  'auth.login': '用户登录',
  'auth.login.success': '登录成功',
  'auth.login.failure': '登录失败',
  'auth.login.rate_limited': '登录请求被限流',
  'auth.account.locked': '账号被锁定',
  'auth.logout': '退出登录',
  'auth.refresh.success': '会话刷新成功',
  'auth.refresh.failure': '会话刷新失败',
  'auth.refresh.replayed': '检测到刷新令牌重放',
  'auth.password.changed': '修改密码成功',
  'auth.password.change_failed': '修改密码失败',
  'user.created': '创建用户',
  'user.updated': '修改用户信息',
  'user.status.changed': '变更用户状态',
  'user.unlocked': '解锁用户',
  'user.role.changed': '变更用户角色',
  'user.password.reset': '重置用户密码',
}

export const auditEventLabel = (eventName: string): string =>
  Object.hasOwn(auditEventLabels, eventName) ? auditEventLabels[eventName]! : '未配置中文名称的事件'

export const auditEventOptions = Object.entries(auditEventLabels).map(([code, label]) => ({
  code,
  label,
}))

export const isAdmin = (role?: Role) => role === 'admin' || role === 'super_admin'

export const manageableRoles = (role?: Role): Role[] =>
  role === 'super_admin'
    ? ['operator', 'engineer', 'admin', 'super_admin']
    : role === 'admin'
      ? ['operator', 'engineer']
      : []

export const formatTime = (value: number | null) =>
  value == null ? '—' : new Date(value).toLocaleString('zh-CN', { hour12: false })

export const toUtcSeconds = (value: string) =>
  value ? new Date(value).toISOString().replace(/\.\d{3}Z$/, 'Z') : ''

export async function hashPassword(password: string): Promise<string> {
  if (!globalThis.crypto?.subtle)
    throw new Error('当前环境不支持安全登录，请使用 HTTPS 或 localhost 访问。')
  const bytes = await crypto.subtle.digest('SHA-256', new TextEncoder().encode(password))
  return Array.from(new Uint8Array(bytes), (byte) => byte.toString(16).padStart(2, '0')).join('')
}

export function passwordPolicy(role?: Role) {
  const simple = role === 'operator' || role === 'engineer'
  return {
    minLength: simple ? 6 : 8,
    requireLettersAndNumbers: !simple,
    hint: simple
      ? '至少 6 个字符，可使用纯数字或纯字母'
      : '至少 8 个字符，包含字母和数字，不强制特殊字符',
  }
}

export function passwordProblem(
  password: string,
  oldPassword: string,
  confirmation: string,
  role?: Role,
): string {
  if (password !== confirmation) return '两次输入的新密码不一致。'
  if (password === oldPassword) return '新密码不能与当前密码相同。'
  const policy = passwordPolicy(role)
  if (
    [...password].length < policy.minLength ||
    (policy.requireLettersAndNumbers && (!/\p{L}/u.test(password) || !/\p{N}/u.test(password)))
  )
    return `新密码${policy.hint}。`
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
