import { api } from './client'
import type { AuditLog, CreateUser, CredentialIssue, Page, User } from './types'

export const adminApi = {
  users: (params: Record<string, string | number>) =>
    api.request<Page<User>>({ url: '/admin/users', params }),
  user: (id: string) => api.request<User>({ url: `/admin/users/${encodeURIComponent(id)}` }),
  create: (data: CreateUser) =>
    api.request<CredentialIssue>({ url: '/admin/users', method: 'POST', data }),
  update: (user: User, action: 'name' | 'status' | 'unlock' | 'role', value?: string) => {
    const url = `/admin/users/${encodeURIComponent(user.id)}${action === 'name' ? '' : `/${action}`}`
    const data =
      action === 'name' ? { display_name: value } : action === 'unlock' ? {} : { [action]: value }
    return api.request<User>({
      url,
      method: action === 'role' ? 'PUT' : 'PATCH',
      headers: { 'If-Match': String(user.version) },
      data,
    })
  },
  resetPassword: (user: User) =>
    api.request<CredentialIssue>({
      url: `/admin/users/${encodeURIComponent(user.id)}/reset-password`,
      method: 'POST',
      headers: { 'If-Match': String(user.version) },
    }),
  logs: (params: Record<string, string | number>) =>
    api.request<Page<AuditLog>>({ url: '/admin/user-logs', params }),
}
