import { computed, onBeforeUnmount, onMounted, reactive, ref, shallowRef, watch } from 'vue'
import { onBeforeRouteLeave } from 'vue-router'
import { useI18n } from 'vue-i18n'
import { api, ApiError, errorMessage } from '../../api/client'
import { rbacApi } from '../../api/rbac'
import type { PermissionRecord, RoleRecord } from '../../api/types'
import { useAuthStore } from '../../stores/auth'
import { useTask } from '../../composables/useTask'

export type RoleAction = 'create' | 'edit' | 'status' | 'archive' | 'permissions' | 'scopes'
const capabilities: Record<RoleAction, string> = {
  create: 'role.create',
  edit: 'role.update',
  status: 'role.status',
  archive: 'role.archive',
  permissions: 'role.permission.assign',
  scopes: 'role.scope.assign',
}

export function useRoles() {
  const auth = useAuthStore()
  const { t } = useI18n()
  const rows = shallowRef<RoleRecord[]>([])
  const filter = reactive({ search: '', status: '', source: '', includeArchived: false })
  const loading = ref(false)
  const listError = ref('')
  const success = ref('')
  const action = ref<RoleAction>()
  const selected = shallowRef<RoleRecord>()
  const draft = reactive({
    code: '',
    name: '',
    description: '',
    confirmation: '',
    ids: [] as string[],
  })
  const permissions = shallowRef<PermissionRecord[]>([])
  const targets = shallowRef<RoleRecord[]>([])
  const preservedIds = shallowRef<string[]>([])
  const ready = ref(false)
  const conflict = ref(false)
  const discardRequested = ref(false)
  const task = useTask()
  let baseline = ''
  let disposed = false
  let listEpoch = 0
  let listRequest: AbortController | undefined
  let editorRequest: AbortController | undefined
  let leave: ((allow: boolean) => void) | undefined
  const fingerprint = () => JSON.stringify({ ...draft, ids: [...draft.ids].sort() })
  const dirty = computed(() => !!action.value && ready.value && fingerprint() !== baseline)
  const visible = computed(() =>
    rows.value.filter(
      (role) =>
        (!filter.status || role.status === filter.status) &&
        (!filter.source || (role.system_key ? 'system' : 'custom') === filter.source) &&
        [role.code, role.name, role.description]
          .join(' ')
          .toLocaleLowerCase()
          .includes(filter.search.trim().toLocaleLowerCase()),
    ),
  )
  function allowed(mode: RoleAction, role?: RoleRecord) {
    if (!auth.can(capabilities[mode])) return false
    if (mode === 'create') return true
    if (!role || role.status === 'archived' || role.manageable !== true) return false
    if (
      auth.permissions?.manageable_roles &&
      !auth.permissions.manageable_roles.includes(role.code)
    )
      return false
    if (mode === 'permissions' && !auth.can('permission.read')) return false
    if (mode === 'archive' && (role.system_key || role.user_count == null || role.user_count > 0))
      return false
    if (mode === 'status' && role.system_key === 'super_admin') return false
    return true
  }
  async function load() {
    listRequest?.abort()
    const epoch = ++listEpoch
    const controller = new AbortController()
    listRequest = controller
    loading.value = true
    listError.value = ''
    try {
      const response = await rbacApi.roles(filter.includeArchived, controller.signal)
      if (!disposed && epoch === listEpoch) rows.value = response.data.items
    } catch (failure) {
      if (!disposed && epoch === listEpoch && !controller.signal.aborted) {
        rows.value = []
        listError.value = errorMessage(failure)
      }
    } finally {
      if (!disposed && epoch === listEpoch) loading.value = false
    }
  }
  async function readEditor() {
    ready.value = false
    conflict.value = false
    const mode = action.value
    const id = selected.value?.id
    if (!mode || mode === 'create' || !id) return
    editorRequest?.abort()
    const controller = new AbortController()
    editorRequest = controller
    const first = (await rbacApi.role(id, controller.signal)).data
    if (!auth.permissions) await auth.loadPermissions()
    if (!allowed(mode, first)) throw new ApiError(2204)
    let ids: string[] = []
    let nextPermissions: PermissionRecord[] = []
    let nextTargets: RoleRecord[] = []
    if (mode === 'permissions') {
      ids = (await rbacApi.rolePermissionIds(id, controller.signal)).data.permission_ids
      nextPermissions = (await rbacApi.permissions(true, controller.signal)).data.items
    } else if (mode === 'scopes') {
      ids = (await rbacApi.roleScopeIds(id, controller.signal)).data.target_role_ids
      nextTargets = (await rbacApi.roles(false, controller.signal)).data.items
    }
    const latest = (await rbacApi.role(id, controller.signal)).data
    if (disposed || controller.signal.aborted || action.value !== mode) return
    if (first.version !== latest.version || first.authz_version !== latest.authz_version) {
      conflict.value = true
      throw new ApiError(2209)
    }
    selected.value = latest
    permissions.value = nextPermissions.filter(
      (permission) => permission.status === 'active' || ids.includes(permission.id),
    )
    targets.value = nextTargets.filter(
      (role) =>
        role.status === 'active' || ids.includes(role.id) || latest.system_key === 'super_admin',
    )
    const known = new Set(
      mode === 'permissions'
        ? permissions.value.map((permission) => permission.id)
        : targets.value.map((role) => role.id),
    )
    preservedIds.value = ids.filter((value) => !known.has(value))
    Object.assign(draft, {
      code: latest.code,
      name: latest.name,
      description: latest.description,
      confirmation: '',
      ids: ids.filter((value) => known.has(value)),
    })
    ready.value = true
    baseline = fingerprint()
  }
  const open = (mode: RoleAction, role?: RoleRecord) =>
    task.run(async () => {
      if (action.value || !allowed(mode, role)) return
      success.value = ''
      selected.value = role
      action.value = mode
      conflict.value = false
      ready.value = false
      Object.assign(draft, { code: '', name: '', description: '', confirmation: '', ids: [] })
      if (mode === 'create') {
        ready.value = true
        baseline = fingerprint()
      } else await readEditor()
    })
  function finishClose() {
    editorRequest?.abort()
    action.value = undefined
    selected.value = undefined
    ready.value = false
    discardRequested.value = false
    task.error.value = ''
    leave?.(true)
    leave = undefined
  }
  function close() {
    if (task.busy.value) return
    if (dirty.value) discardRequested.value = true
    else finishClose()
  }
  function keepEditing() {
    discardRequested.value = false
    leave?.(false)
    leave = undefined
  }
  const reload = () => task.run(readEditor)
  const submit = () =>
    task.run(async () => {
      const mode = action.value
      const role = selected.value
      if (!mode || !ready.value || conflict.value || !allowed(mode, role)) return
      if (mode === 'archive' && draft.confirmation !== role?.code) return
      try {
        if (mode === 'create')
          await rbacApi.createRole({
            code: draft.code.trim(),
            name: draft.name.trim(),
            description: draft.description.trim(),
          })
        else if (!role) return
        else if (mode === 'edit')
          await rbacApi.updateRole(role, {
            name: draft.name.trim(),
            description: draft.description.trim(),
          })
        else if (mode === 'status')
          await rbacApi.roleStatus(role, role.status === 'active' ? 'disabled' : 'active')
        else if (mode === 'archive') await rbacApi.archiveRole(role)
        else if (mode === 'permissions')
          await rbacApi.replaceRolePermissions(
            role,
            [...new Set([...preservedIds.value, ...draft.ids])].sort(),
          )
        else
          await rbacApi.replaceRoleScopes(
            role,
            [...new Set([...preservedIds.value, ...draft.ids])].sort(),
          )
      } catch (failure) {
        if (failure instanceof ApiError && failure.code === 2209) conflict.value = true
        if (failure instanceof ApiError && [2204, 2205, 2229, 2231].includes(failure.code))
          ready.value = false
        throw failure
      }
      if (disposed) return
      finishClose()
      success.value = t('rbac.saved')
      await api.refresh()
      await auth.loadPermissions()
      await load()
    })
  const requiredPermission = (permission: PermissionRecord) =>
    selected.value?.system_key === 'super_admin' && permission.required_for_super_admin
  const requiredScope = () => selected.value?.system_key === 'super_admin'
  onBeforeRouteLeave(() => {
    if (!auth.user || auth.needsPassword || (auth.permissions && !auth.can('role.read')))
      return true
    if (task.busy.value) return false
    if (!dirty.value) return true
    discardRequested.value = true
    leave?.(false)
    return new Promise<boolean>((resolve) => {
      leave = resolve
    })
  })
  function beforeUnload(event: BeforeUnloadEvent) {
    if (dirty.value || task.busy.value) {
      event.preventDefault()
      event.returnValue = ''
    }
  }
  watch(
    () => filter.includeArchived,
    () => void load(),
  )
  onMounted(() => {
    void load()
    window.addEventListener('beforeunload', beforeUnload)
  })
  onBeforeUnmount(() => {
    disposed = true
    listEpoch++
    listRequest?.abort()
    editorRequest?.abort()
    leave?.(false)
    window.removeEventListener('beforeunload', beforeUnload)
  })
  return {
    rows,
    visible,
    filter,
    loading,
    listError,
    success,
    action,
    selected,
    draft,
    permissions,
    targets,
    preservedIds,
    ready,
    conflict,
    discardRequested,
    task,
    can: auth.can,
    allowed,
    open,
    load,
    reload,
    close,
    finishClose,
    keepEditing,
    submit,
    requiredPermission,
    requiredScope,
  }
}
