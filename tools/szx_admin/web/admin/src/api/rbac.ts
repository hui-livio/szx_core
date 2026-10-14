import { api } from './client'
import type { NewPermission, PermissionRecord, RoleRecord } from './types'

const headers = (version: number) => ({ 'If-Match': String(version) })
export const rbacApi = {
  permissions: (includeArchived: boolean, signal?: AbortSignal) =>
    api.request<{ items: PermissionRecord[] }>({
      url: '/admin/permissions',
      params: { include_archived: includeArchived },
      signal,
    }),
  permission: (id: string) =>
    api.request<PermissionRecord>({ url: `/admin/permissions/${encodeURIComponent(id)}` }),
  createPermission: (data: NewPermission) =>
    api.request<PermissionRecord>({ url: '/admin/permissions', method: 'POST', data }),
  updatePermission: (
    permission: PermissionRecord,
    data: Pick<NewPermission, 'name' | 'description'>,
  ) =>
    api.request<PermissionRecord>({
      url: `/admin/permissions/${encodeURIComponent(permission.id)}`,
      method: 'PATCH',
      headers: headers(permission.version),
      data,
    }),
  permissionStatus: (permission: PermissionRecord, status: 'active' | 'disabled') =>
    api.request<PermissionRecord>({
      url: `/admin/permissions/${encodeURIComponent(permission.id)}/status`,
      method: 'PATCH',
      headers: headers(permission.version),
      data: { status },
    }),
  archivePermission: (permission: PermissionRecord) =>
    api.request<void>({
      url: `/admin/permissions/${encodeURIComponent(permission.id)}`,
      method: 'DELETE',
      headers: headers(permission.version),
    }),
  roles: (includeArchived = false, signal?: AbortSignal) =>
    api.request<{ items: RoleRecord[] }>({
      url: '/admin/roles',
      params: { include_archived: includeArchived },
      signal,
    }),
  role: (id: string, signal?: AbortSignal) =>
    api.request<RoleRecord>({ url: `/admin/roles/${encodeURIComponent(id)}`, signal }),
  rolePermissionIds: (id: string, signal?: AbortSignal) =>
    api.request<{ permission_ids: string[] }>({
      url: `/admin/roles/${encodeURIComponent(id)}/permissions`,
      signal,
    }),
  replaceRolePermissions: (role: RoleRecord, permission_ids: string[]) =>
    api.request<RoleRecord>({
      url: `/admin/roles/${encodeURIComponent(role.id)}/permissions`,
      method: 'PUT',
      headers: headers(role.version),
      data: { permission_ids },
    }),
}
