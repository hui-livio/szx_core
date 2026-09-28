import { computed, onMounted, onUnmounted, reactive, shallowRef } from 'vue'
import { adminApi } from '../api/admin'
import { ApiError } from '../api/client'
import type { CredentialIssue, Role, User, UserStatus } from '../api/types'
import { hashPassword, manageableRoles, passwordPolicy, passwordProblem } from '../domain'
import { useAuthStore } from '../stores/auth'
import { usePagedQuery } from './usePagedQuery'
import { useTask } from './useTask'
import { translate } from '../i18n'
import { useI18n } from 'vue-i18n'

export type UserAction = 'create' | 'details' | 'name' | 'role' | 'status' | 'unlock' | 'reset'
export type UserRowView = User & {
  canManage: boolean
  canAssignRole: boolean
  canChangeStatus: boolean
}
export interface UserDraft {
  identity: string
  displayName: string
  role: Role
  password: string
  confirmation: string
}
export const actionLabels: Record<UserAction, string> = {
  create: 'users.createAction',
  details: 'users.detailsAction',
  name: 'users.nameAction',
  role: 'users.roleAction',
  status: 'users.statusAction',
  unlock: 'users.unlockAction',
  reset: 'users.resetAction',
}

export function useUsersViewModel() {
  const auth = useAuthStore()
  const { t } = useI18n()
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
  const canAssignRole = computed(
    () => auth.user?.role === 'admin' || auth.user?.role === 'super_admin',
  )
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
  const dialogTitle = computed(() => (action.value ? t(actionLabels[action.value]) : ''))
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
      if (conflict.value) throw new Error(translate('errors.reloadUser'))
      try {
        const operation = action.value
        if (!operation || operation === 'details') return
        let changed: User
        let requestId: string
        if (operation === 'create') {
          const identity = draft.identity.trim().normalize('NFC')
          if (!identity || [...identity].length > 64)
            throw new Error(translate('errors.identityLength'))
          if (!draft.displayName.trim() || [...draft.displayName].length > 128)
            throw new Error(translate('errors.displayNameLength'))
          if (!roles.value.includes(draft.role))
            throw new Error(translate('errors.invalidRoleCreate'))
          const passwordHash = await draftPasswordHash(draft.role)
          const response = await adminApi.create({
            login_identifier: identity,
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
          if (!roles.value.includes(user.role)) throw new Error(translate('errors.manageForbidden'))
          if (user.id === auth.user?.id && (operation === 'role' || operation === 'status'))
            throw new Error(
              translate(operation === 'role' ? 'errors.selfRole' : 'errors.selfDisable'),
            )
          if (operation === 'role' && (!canAssignRole.value || !roles.value.includes(draft.role)))
            throw new Error(translate('errors.roleForbidden'))
          if (operation === 'reset') {
            const response = await adminApi.resetPassword(user, await draftPasswordHash(user.role))
            clearPassword()
            credential.value = response.data.temporary_credential ? response.data : null
            changed = response.data.user
            requestId = response.request_id
          } else {
            if (operation === 'name' && !draft.displayName.trim())
              throw new Error(translate('errors.displayNameRequired'))
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
        success.value = t('users.success', { action: t(actionLabels[operation]), requestId })
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
