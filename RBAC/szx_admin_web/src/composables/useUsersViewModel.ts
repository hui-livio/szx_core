import { computed, onMounted, onUnmounted, reactive, shallowRef } from 'vue'
import { adminApi } from '../api/admin'
import { ApiError } from '../api/client'
import type { CredentialIssue, Role, User, UserStatus } from '../api/types'
import { hashPassword, manageableRoles, passwordPolicy, passwordProblem } from '../domain'
import { useAuthStore } from '../stores/auth'
import { usePagedQuery } from './usePagedQuery'
import { useTask } from './useTask'

export type UserAction = 'create' | 'details' | 'name' | 'role' | 'status' | 'unlock' | 'reset'
export type UserRowView = User & {
  canManage: boolean
  canAssignRole: boolean
  canChangeStatus: boolean
}
export interface UserDraft {
  identityType: 'username' | 'uid'
  identity: string
  displayName: string
  role: Role
  password: string
  confirmation: string
}
export const actionLabels: Record<UserAction, string> = {
  create: '创建用户',
  details: '用户详情',
  name: '编辑展示名',
  role: '分配角色',
  status: '变更用户状态',
  unlock: '解锁用户',
  reset: '重置密码',
}

export function useUsersViewModel() {
  const auth = useAuthStore()
  const filters = reactive({
    search: '',
    role: '' as Role | '',
    status: '' as UserStatus | '',
    sort: '-created_at',
  })
  let applied = { ...filters }
  const query = usePagedQuery((page, page_size) =>
    adminApi.users({
      page,
      page_size,
      ...Object.fromEntries(Object.entries(applied).filter(([, value]) => value !== '')),
    }),
  )
  const mutation = useTask()
  const action = shallowRef<UserAction | null>(null)
  const selected = shallowRef<User | null>(null)
  const credential = shallowRef<CredentialIssue | null>(null)
  const credentialRequestId = shallowRef('')
  const success = shallowRef('')
  const conflict = shallowRef(false)
  const draft = reactive<UserDraft>({
    identityType: 'username',
    identity: '',
    displayName: '',
    role: 'operator',
    password: '',
    confirmation: '',
  })
  const roles = computed(() => manageableRoles(auth.user?.role))
  const passwordHint = computed(
    () => passwordPolicy(action.value === 'reset' ? selected.value?.role : draft.role).hint,
  )
  const canAssignRole = computed(() => auth.user?.role === 'super_admin')
  const users = computed<UserRowView[]>(() =>
    query.items.value.map((user) => {
      const canManage = roles.value.includes(user.role)
      const isSelf = user.id === auth.user?.id
      return {
        ...user,
        canManage,
        canAssignRole: canManage && canAssignRole.value && !isSelf,
        canChangeStatus: canManage && !isSelf,
      }
    }),
  )
  const dialogTitle = computed(() => (action.value ? actionLabels[action.value] : ''))
  const targetStatus = computed(() =>
    selected.value?.status === 'disabled' ? 'active' : 'disabled',
  )

  function search() {
    applied = { ...filters }
    return query.load(1)
  }
  function close() {
    if (!mutation.busy.value) {
      clearPassword()
      action.value = null
      selected.value = null
      mutation.error.value = ''
      conflict.value = false
    }
  }
  function open(next: UserAction, user?: User) {
    if (mutation.busy.value) return
    clearPassword()
    action.value = next
    selected.value = user ?? null
    mutation.error.value = ''
    conflict.value = false
    draft.identityType = 'username'
    draft.identity = ''
    draft.displayName = user?.display_name ?? ''
    draft.role = user?.role ?? 'operator'
    if (user) void reloadSelected()
  }
  const reloadSelected = () =>
    mutation.run(async () => {
      if (!selected.value) return
      const response = await adminApi.user(selected.value.id)
      selected.value = response.data
      draft.displayName = response.data.display_name
      draft.role = response.data.role
      conflict.value = false
    })
  function closeCredential() {
    const user = credential.value?.user
    credential.value = null
    credentialRequestId.value = ''
    if (user) auth.updateSelf(user)
  }
  function clearPassword() {
    draft.password = ''
    draft.confirmation = ''
  }
  async function draftPasswordHash(role: Role) {
    if (draft.password || draft.confirmation) {
      const problem = passwordProblem(draft.password, '', draft.confirmation, role)
      if (problem) throw new Error(problem)
    }
    return draft.password ? hashPassword(draft.password) : undefined
  }
  const submit = () =>
    mutation.run(async () => {
      if (conflict.value) throw new Error('请先重新加载用户，再确认操作。')
      try {
        const operation = action.value
        if (!operation || operation === 'details') return
        let changed: User
        let requestId: string
        if (operation === 'create') {
          const identity = draft.identity.trim().normalize('NFC')
          if (!identity || [...identity].length > 64) throw new Error('登录标识须为 1–64 个字符。')
          if (!draft.displayName.trim() || [...draft.displayName].length > 128)
            throw new Error('展示名须为 1–128 个字符。')
          if (!roles.value.includes(draft.role)) throw new Error('不能创建此角色的用户。')
          const passwordHash = await draftPasswordHash(draft.role)
          const response = await adminApi.create({
            ...(draft.identityType === 'uid' ? { uid: identity } : { username: identity }),
            display_name: draft.displayName,
            role: draft.role,
            ...(passwordHash ? { password_hash: passwordHash } : {}),
          })
          clearPassword()
          credential.value = response.data.temporary_credential ? response.data : null
          changed = response.data.user
          requestId = response.request_id
        } else {
          const user = selected.value
          if (!user) return
          if (!roles.value.includes(user.role)) throw new Error('无权管理此用户。')
          if (user.id === auth.user?.id && (operation === 'role' || operation === 'status'))
            throw new Error(operation === 'role' ? '不能给自己分配角色。' : '不能禁用自己。')
          if (operation === 'role' && (!canAssignRole.value || !roles.value.includes(draft.role)))
            throw new Error('无权分配此角色。')
          if (operation === 'reset') {
            const response = await adminApi.resetPassword(user, await draftPasswordHash(user.role))
            clearPassword()
            credential.value = response.data.temporary_credential ? response.data : null
            changed = response.data.user
            requestId = response.request_id
          } else {
            if (operation === 'name' && !draft.displayName.trim()) throw new Error('请填写展示名。')
            const value =
              operation === 'name'
                ? draft.displayName
                : operation === 'role'
                  ? draft.role
                  : targetStatus.value
            const response = await adminApi.update(user, operation, value)
            changed = response.data
            requestId = response.request_id
          }
        }
        success.value = `${actionLabels[operation]}成功 · 请求 ID：${requestId}`
        credentialRequestId.value = credential.value ? requestId : ''
        action.value = null
        selected.value = null
        if (!credential.value) auth.updateSelf(changed)
        // A self-reset invalidates our access token. Keep the one-time credential
        // visible until acknowledged, instead of triggering reauthentication here.
        const selfCredentialChange = credential.value && changed.id === auth.user?.id
        if (auth.user && !selfCredentialChange) await query.load()
      } catch (error) {
        if (error instanceof ApiError && error.code === 2209) conflict.value = true
        throw error
      }
    })
  onMounted(() => query.load())
  onUnmounted(() => {
    clearPassword()
    credential.value = null
  })
  return {
    filters,
    users,
    query,
    mutation,
    action,
    selected,
    credential,
    credentialRequestId,
    success,
    conflict,
    draft,
    roles,
    passwordHint,
    canAssignRole,
    dialogTitle,
    targetStatus,
    search,
    open,
    close,
    reloadSelected,
    closeCredential,
    submit,
  }
}
