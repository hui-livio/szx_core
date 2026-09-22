export type Role = 'operator' | 'engineer' | 'admin' | 'super_admin'
export type UserStatus = 'active' | 'disabled' | 'locked'
export interface User {
  id: string
  uid?: string | null
  username?: string | null
  display_name: string
  role: Role
  status: UserStatus
  force_password_change: boolean
  password_change_recommended: boolean
  version: number
  authz_version: number
  last_login_at: number | null
  created_at: number
  updated_at: number
}
export interface Session {
  user: User
  access_token: string
  csrf_token: string
  expires_at: number
  expires_in: number
  authz_version: number
  token_type: 'Bearer'
}
export interface Envelope<T> {
  data: T
  request_id: string
}
export interface Page<T> {
  items: T[]
  total: number
  page: number
  page_size: number
}
export interface CredentialIssue {
  user: User
  temporary_credential: string | null
  temporary_credential_expires_at: number | null
}
export interface Permissions {
  user_id: string
  role: Role
  status: UserStatus
  force_password_change: boolean
  authz_version: number
  capabilities: string[]
}
export interface AuditLog {
  id: string
  actor_user_id: string
  actor_username: string
  target_user_id: string
  event_name: string
  result: 'success' | 'failure' | 'denied'
  request_id: string
  peer_address: string
  details: Record<string, unknown>
  created_at: number
}
export interface BehaviorEventRecord {
  id: string
  user_id: string
  username: string
  event_name: string
  result: 'success' | 'failure'
  request_id: string
  details: Record<string, unknown>
  created_at: number
}
export type CreateUser = { display_name: string; role: Role; password_hash?: string } & (
  { username: string; uid?: never } | { uid: string; username?: never }
)
