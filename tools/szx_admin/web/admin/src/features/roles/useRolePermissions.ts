import { computed, onBeforeUnmount, onMounted, reactive, ref, shallowRef, watch } from 'vue'
import { useAuthStore } from '../../stores/auth'
import { rbacApi } from '../../api/rbac'
import { ApiError, errorMessage } from '../../api/client'
import type { PermissionRecord, RoleRecord } from '../../api/types'
import { useRoute } from 'vue-router'

export interface RolePermissionView {
  id: string
  permission?: PermissionRecord
  effective: boolean | null
}

export function useRolePermissions() {
  const auth = useAuthStore()
  const route = useRoute()
  const roles = shallowRef<RoleRecord[]>([])
  const roleId = ref(typeof route.query.role === 'string' ? route.query.role : '')
  const selectedRole = shallowRef<RoleRecord>()
  const ids = shallowRef<string[]>([])
  const catalog = shallowRef(new Map<string, PermissionRecord>())
  const catalogAvailable = ref(false)
  const catalogError = ref('')
  const listError = ref('')
  const detailError = ref('')
  const loading = ref(false)
  const reading = ref(false)
  const filters = reactive({
    search: '',
    source: '',
    includeArchived: route.query.archived === 'true',
  })
  let listEpoch = 0
  let detailEpoch = 0
  let listRequest: AbortController | undefined
  let detailRequest: AbortController | undefined
  let disposed = false
  function invalidateDetail() {
    detailEpoch++
    detailRequest?.abort()
    selectedRole.value = undefined
    ids.value = []
    detailError.value = ''
    catalog.value = new Map()
    catalogAvailable.value = false
    catalogError.value = ''
    reading.value = false
  }
  const linked = computed<RolePermissionView[]>(() =>
    ids.value.map((id) => {
      const permission = catalog.value.get(id)
      const effective =
        selectedRole.value?.status !== 'active'
          ? false
          : permission
            ? permission.status === 'active'
            : null
      return { id, permission, effective }
    }),
  )
  const visible = computed(() =>
    linked.value.filter((item) => {
      const permission = item.permission
      return (
        (!filters.source || permission?.source === filters.source) &&
        [item.id, permission?.code, permission?.name, permission?.description, permission?.module]
          .join(' ')
          .toLocaleLowerCase()
          .includes(filters.search.trim().toLocaleLowerCase())
      )
    }),
  )
  const effectiveCount = computed(() =>
    linked.value.some((item) => item.effective === null)
      ? null
      : linked.value.filter((item) => item.effective).length,
  )
  async function readSelected() {
    invalidateDetail()
    const id = roleId.value
    if (disposed || !id) return
    const epoch = detailEpoch
    const controller = new AbortController()
    detailRequest = controller
    reading.value = true
    const current = () =>
      !disposed && epoch === detailEpoch && roleId.value === id && !controller.signal.aborted
    try {
      const first = (await rbacApi.role(id, controller.signal)).data
      const assignments = (await rbacApi.rolePermissionIds(id, controller.signal)).data
        .permission_ids
      let nextCatalog = new Map<string, PermissionRecord>()
      let nextCatalogAvailable = false
      let metadataError = ''
      if (auth.can('permission.read')) {
        try {
          const response = await rbacApi.permissions(true, controller.signal)
          nextCatalog = new Map(
            response.data.items.map((permission) => [permission.id, permission]),
          )
          nextCatalogAvailable = true
        } catch (failure) {
          metadataError = errorMessage(failure)
        }
      }
      const latest = (await rbacApi.role(id, controller.signal)).data
      if (!current()) return
      // Avoid presenting a role version from one authorization state with IDs from another.
      if (first.version !== latest.version || first.authz_version !== latest.authz_version)
        throw new ApiError(2209)
      catalog.value = nextCatalog
      catalogAvailable.value = nextCatalogAvailable
      catalogError.value = metadataError
      if (!nextCatalogAvailable) filters.source = ''
      selectedRole.value = latest
      ids.value = [...new Set(assignments)]
    } catch (failure) {
      if (current()) detailError.value = errorMessage(failure)
    } finally {
      if (current()) reading.value = false
    }
  }
  async function load() {
    listRequest?.abort()
    invalidateDetail()
    const epoch = ++listEpoch
    const controller = new AbortController()
    listRequest = controller
    loading.value = true
    listError.value = ''
    catalogError.value = ''
    catalogAvailable.value = false
    catalog.value = new Map()
    const current = () => !disposed && epoch === listEpoch && !controller.signal.aborted
    try {
      const response = await rbacApi.roles(filters.includeArchived, controller.signal)
      if (!current()) return
      roles.value = response.data.items
      if (!current()) return
      const nextId = roles.value.some((role) => role.id === roleId.value)
        ? roleId.value
        : (roles.value[0]?.id ?? '')
      if (nextId !== roleId.value) roleId.value = nextId
      else await readSelected()
    } catch (failure) {
      if (current()) {
        roles.value = []
        roleId.value = ''
        listError.value = errorMessage(failure)
      }
    } finally {
      if (current()) loading.value = false
    }
  }
  watch(roleId, () => {
    if (!disposed) void readSelected()
  })
  watch(
    () => filters.includeArchived,
    () => void load(),
  )
  onMounted(() => void load())
  onBeforeUnmount(() => {
    disposed = true
    listEpoch++
    listRequest?.abort()
    invalidateDetail()
  })
  return {
    roles,
    roleId,
    selectedRole,
    linked,
    visible,
    filters,
    catalogAvailable,
    catalogError,
    listError,
    detailError,
    loading,
    reading,
    effectiveCount,
    load,
    readSelected,
    canManagePermissions: computed(() => auth.can('permission.read')),
  }
}
