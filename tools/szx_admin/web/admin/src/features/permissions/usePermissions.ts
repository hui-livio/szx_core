import { computed, onBeforeUnmount, onMounted, reactive, ref, shallowRef, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import { useAuthStore } from '../../stores/auth'
import { useTask } from '../../composables/useTask'
import { ApiError, errorMessage } from '../../api/client'
import { rbacApi } from '../../api/rbac'
import type { PermissionRecord, RoleRecord } from '../../api/types'

export function usePermissions() {
  const auth = useAuthStore()
  const { t } = useI18n()
  const rows = shallowRef<PermissionRecord[]>([])
  const roles = shallowRef<RoleRecord[]>([])
  const filter = reactive({ search: '', source: '', includeArchived: false })
  const loading = ref(false)
  const error = ref('')
  const success = ref('')
  const action = ref<'create' | 'edit' | 'status' | 'archive' | 'assign'>()
  const selected = shallowRef<PermissionRecord>()
  const selectedRole = shallowRef<RoleRecord>()
  const assigned = ref<string[]>([])
  const preserved = ref<string[]>([])
  const roleId = ref('')
  const conflict = ref(false)
  const draft = reactive({ code: '', name: '', module: '', description: '' })
  const mutation = useTask()
  let epoch = 0
  let request: AbortController | undefined
  let disposed = false
  const visible = computed(() =>
    rows.value.filter(
      (item) =>
        (!filter.source || item.source === filter.source) &&
        [item.code, item.name, item.module, item.description]
          .join(' ')
          .toLocaleLowerCase()
          .includes(filter.search.trim().toLocaleLowerCase()),
    ),
  )
  const can = auth.can
  const assignmentOptions = computed(() =>
    rows.value.filter((item) => item.status === 'active' && item.source === 'custom'),
  )
  async function load() {
    request?.abort()
    const generation = ++epoch
    const controller = new AbortController()
    request = controller
    loading.value = true
    error.value = ''
    try {
      const response = await rbacApi.permissions(filter.includeArchived, controller.signal)
      if (!disposed && generation === epoch) rows.value = response.data.items
    } catch (failure) {
      if (!disposed && generation === epoch && !controller.signal.aborted)
        error.value = errorMessage(failure)
    } finally {
      if (!disposed && generation === epoch) loading.value = false
    }
  }
  function close() {
    if (mutation.busy.value) return
    action.value = undefined
    selected.value = undefined
    selectedRole.value = undefined
    roles.value = []
    assigned.value = []
    preserved.value = []
    roleId.value = ''
    conflict.value = false
    mutation.error.value = ''
  }
  function open(mode: 'create' | 'edit' | 'status' | 'archive', item?: PermissionRecord) {
    selected.value = item
    action.value = mode
    conflict.value = false
    mutation.error.value = ''
    Object.assign(draft, {
      code: item?.code ?? '',
      name: item?.name ?? '',
      module: item?.module ?? '',
      description: item?.description ?? '',
    })
  }
  async function loadRole() {
    selectedRole.value = undefined
    assigned.value = []
    preserved.value = []
    if (!roleId.value) return
    const id = roleId.value
    const first = (await rbacApi.role(id)).data
    const ids = (await rbacApi.rolePermissionIds(id)).data.permission_ids
    const latest = (await rbacApi.role(id)).data
    if (first.version !== latest.version) {
      conflict.value = true
      throw new ApiError(2209)
    }
    if (disposed || roleId.value !== id) return
    selectedRole.value = latest
    const editable = new Set(assignmentOptions.value.map((item) => item.id))
    assigned.value = ids.filter((value) => editable.has(value))
    // System, disabled and otherwise hidden assignments must never be silently removed.
    preserved.value = ids.filter((value) => !editable.has(value))
    conflict.value = false
  }
  const chooseRole = () => mutation.run(loadRole)
  const openAssignment = () =>
    mutation.run(async () => {
      success.value = ''
      conflict.value = false
      roles.value = (await rbacApi.roles()).data.items.filter((item) => item.status === 'active')
      roleId.value = ''
      selectedRole.value = undefined
      assigned.value = []
      preserved.value = []
      action.value = 'assign'
    })
  const reloadSelected = () =>
    mutation.run(async () => {
      if (action.value === 'assign') {
        await loadRole()
        return
      }
      if (!selected.value) return
      const latest = (await rbacApi.permission(selected.value.id)).data
      selected.value = latest
      Object.assign(draft, {
        code: latest.code,
        name: latest.name,
        module: latest.module,
        description: latest.description,
      })
      conflict.value = false
    })
  const submit = () =>
    mutation.run(async () => {
      if (conflict.value || !action.value) return
      try {
        const mode = action.value
        if (mode === 'create')
          await rbacApi.createPermission({
            code: draft.code.trim(),
            name: draft.name.trim(),
            module: draft.module.trim(),
            description: draft.description.trim(),
          })
        else if (mode === 'edit' && selected.value)
          await rbacApi.updatePermission(selected.value, {
            name: draft.name.trim(),
            description: draft.description.trim(),
          })
        else if (mode === 'status' && selected.value)
          await rbacApi.permissionStatus(
            selected.value,
            selected.value.status === 'active' ? 'disabled' : 'active',
          )
        else if (mode === 'archive' && selected.value)
          await rbacApi.archivePermission(selected.value)
        else if (mode === 'assign' && selectedRole.value)
          await rbacApi.replaceRolePermissions(
            selectedRole.value,
            [...new Set([...preserved.value, ...assigned.value])].sort(),
          )
        else return
        if (disposed) return
        action.value = undefined
        success.value = t('rbac.saved')
        await load()
      } catch (failure) {
        if (failure instanceof ApiError && failure.code === 2209) conflict.value = true
        throw failure
      }
    })
  watch(
    () => filter.includeArchived,
    () => void load(),
  )
  onMounted(() => void load())
  onBeforeUnmount(() => {
    disposed = true
    epoch++
    request?.abort()
  })
  return {
    rows,
    roles,
    filter,
    visible,
    loading,
    error,
    success,
    action,
    selected,
    selectedRole,
    assigned,
    roleId,
    conflict,
    draft,
    mutation,
    can,
    assignmentOptions,
    load,
    open,
    close,
    openAssignment,
    chooseRole,
    reloadSelected,
    submit,
  }
}
