import { api } from './client'
import type { NewPermission, NewRole, PermissionRecord, RoleRecord } from './types'

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
  createRole: (data: NewRole) =>
    api.request<RoleRecord>({ url: '/admin/roles', method: 'POST', data }),
  updateRole: (role: RoleRecord, data: Pick<NewRole, 'name' | 'description'>) =>
    api.request<RoleRecord>({
      url: `/admin/roles/${encodeURIComponent(role.id)}`,
      method: 'PATCH',
      headers: headers(role.version),
      data,
    }),
  roleStatus: (role: RoleRecord, status: 'active' | 'disabled') =>
    api.request<RoleRecord>({
      url: `/admin/roles/${encodeURIComponent(role.id)}/status`,
      method: 'PATCH',
      headers: headers(role.version),
      data: { status },
    }),
  archiveRole: (role: RoleRecord) =>
    api.request<void>({
      url: `/admin/roles/${encodeURIComponent(role.id)}`,
      method: 'DELETE',
      headers: headers(role.version),
    }),
  roleScopeIds: (id: string, signal?: AbortSignal) =>
    api.request<{ target_role_ids: string[] }>({
      url: `/admin/roles/${encodeURIComponent(id)}/manage-scopes`,
      signal,
    }),
  replaceRoleScopes: (role: RoleRecord, target_role_ids: string[]) =>
    api.request<RoleRecord>({
      url: `/admin/roles/${encodeURIComponent(role.id)}/manage-scopes`,
      method: 'PUT',
      headers: headers(role.version),
      data: { target_role_ids },
    }),
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
