export type Role = string
export type UserStatus = 'active' | 'disabled' | 'locked'
export interface User {
  id: string
  login_identifier: string
  display_name: string
  role: Role
  status: UserStatus
  force_password_change: boolean
  password_change_recommended: boolean
  version: number
  authz_version: number
  role_id?: string | null
  role_authz_version?: number
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
  manageable_roles?: string[]
  assignable_roles?: string[]
}
export interface AuditLog {
  id: string
  actor_user_id: string
  actor_login_identifier: string
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
  login_identifier: string
  event_name: string
  result: 'success' | 'failure'
  request_id: string
  details: Record<string, unknown>
  created_at: number
}
export type CreateUser = {
  display_name: string
  role: Role
  login_identifier: string
  password_hash?: string
}

export interface MachineMetric {
  status: 'ok' | 'warming_up' | 'unavailable'
  error_code: number | null
  usage_percent: number | null
  total_bytes: number | null
  used_bytes: number | null
  available_bytes: number | null
  free_bytes: number | null
  temperature_celsius: number | null
}
export interface MachineSample {
  sequence: number
  sampled_at: number
  cpu: MachineMetric
  memory: MachineMetric
  gpu: MachineMetric
}
export interface MachineDisk {
  id: string
  path: string
  sampled_at: number
  capacity: MachineMetric
  type: string
}
export interface MachineGpuEngine {
  status: 'ok' | 'warming_up' | 'unavailable'
  error_code: number | null
  total_percent: number | null
  cuda_percent: number | null
  graphics_3d_percent: number | null
}
export interface MachineNetwork {
  id: string
  name: string
  description: string
  ipv4: string
  ipv6: string
  sampled_at: number
  rx_bytes_per_sec: number | null
  tx_bytes_per_sec: number | null
  link_mbps: number | null
}
export interface MachineProcess {
  name: string
  status: 'ok' | 'stopped' | 'unavailable'
  error_code: number | null
  cpu_percent: number | null
  memory_bytes: number | null
}
export interface MachineSnapshot extends MachineSample {
  instance_id: string
  platform: 'windows' | 'linux'
  logical_cpus: number
  sample_interval_ms: number
  disk_interval_ms: number
  disks: MachineDisk[]
  cpu_name: string
  memory_spec: string
  gpu_engine: MachineGpuEngine
  gpu_count: number
  gpu_name: string
  networks: MachineNetwork[]
  processes: MachineProcess[]
}
export interface MachineHistory {
  instance_id: string
  sample_interval_ms: number
  window_seconds: number
  samples: MachineSample[]
}

export interface PermissionRecord {
  id: string
  code: string
  name: string
  description: string
  module: string
  enforcement: 'server' | 'client'
  source: 'system' | 'custom'
  status: 'active' | 'disabled' | 'archived'
  required_for_super_admin: boolean
  version: number
}
export interface RoleRecord {
  id: string
  code: string
  name: string
  description: string
  system_key: string | null
  status: 'active' | 'disabled' | 'archived'
  version: number
  authz_version: number
  user_count?: number
  permission_count?: number
  manageable?: boolean
}

export type NewRole = Pick<RoleRecord, 'code' | 'name' | 'description'>
export interface NewPermission {
  code: string
  name: string
  description: string
  module: string
}
