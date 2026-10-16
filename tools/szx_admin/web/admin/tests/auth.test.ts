import { afterEach, expect, it, vi } from 'vitest'
import { createPinia, setActivePinia } from 'pinia'
import { api } from '../src/api/client'
import { useAuthStore } from '../src/stores/auth'
import type { Envelope, Permissions, Session } from '../src/api/types'

afterEach(() => {
  api.setSession(null)
  vi.restoreAllMocks()
})

function sessionFor(version = 1, id = 'actor'): Session {
  return {
    user: {
      id,
      login_identifier: id,
      display_name: id,
      role: 'custom_manager',
      status: 'active',
      version: 1,
      authz_version: 1,
      role_id: 'role-1',
      role_authz_version: version,
      force_password_change: false,
      password_change_recommended: false,
      created_at: 0,
      updated_at: 0,
      last_login_at: null,
    },
    access_token: `access-${id}-${version}`,
    csrf_token: 'csrf',
    token_type: 'Bearer',
    expires_at: Date.now() + 900000,
    expires_in: 900,
    authz_version: 1,
  }
}

function permissionsFor(capabilities: string[], id = 'actor'): Envelope<Permissions> {
  return {
    data: {
      user_id: id,
      role: 'custom_manager',
      status: 'active',
      force_password_change: false,
      authz_version: 1,
      capabilities,
      manageable_roles: [],
    },
    request_id: 'permissions',
  }
}

function pendingResponse() {
  let resolve!: (response: Envelope<Permissions>) => void
  let reject!: (failure: Error) => void
  const promise = new Promise<Envelope<Permissions>>((complete, fail) => {
    resolve = complete
    reject = fail
  })
  return { promise, resolve, reject }
}

it('invalidates cached capabilities when only the role authorization version changes', () => {
  setActivePinia(createPinia())
  const auth = useAuthStore()
  const session: Session = {
    user: {
      id: 'actor',
      login_identifier: 'admin',
      display_name: 'Admin',
      role: 'custom_manager',
      status: 'active',
      version: 1,
      authz_version: 1,
      role_id: 'role-1',
      role_authz_version: 1,
      force_password_change: false,
      password_change_recommended: false,
      created_at: 0,
      updated_at: 0,
      last_login_at: null,
    },
    access_token: 'first',
    csrf_token: 'csrf',
    token_type: 'Bearer',
    expires_at: Date.now() + 900000,
    expires_in: 900,
    authz_version: 1,
  }
  api.setSession(session)
  auth.permissions = {
    user_id: 'actor',
    role: 'custom_manager',
    status: 'active',
    force_password_change: false,
    authz_version: 1,
    capabilities: ['role.create'],
    manageable_roles: ['reviewer'],
  }
  api.setSession({ ...session, access_token: 'rotated' })
  expect(auth.can('role.create')).toBe(true)
  api.setSession({
    ...session,
    access_token: 'new-authorization',
    user: { ...session.user, role_authz_version: 2 },
  })
  expect(auth.permissions).toBeNull()
  expect(auth.can('role.create')).toBe(false)
  api.setSession(null)
})

it.each(['old-first', 'old-last'] as const)(
  'isolates old authorization responses when they arrive %s',
  async (order) => {
    setActivePinia(createPinia())
    const auth = useAuthStore()
    api.setSession(sessionFor())
    const old = pendingResponse()
    const current = pendingResponse()
    const request = vi
      .spyOn(api, 'request')
      .mockImplementationOnce(() => old.promise)
      .mockImplementationOnce(() => current.promise)
    const obsoleteLoad = auth.loadPermissions()
    api.setSession(sessionFor(2))
    const currentLoad = auth.loadPermissions()
    if (order === 'old-first') {
      old.resolve(permissionsFor(['role.read', 'role.create']))
      await Promise.resolve()
      expect(auth.permissions).toBeNull()
    }
    current.resolve(permissionsFor(['role.read']))
    await currentLoad
    if (order === 'old-last') old.resolve(permissionsFor(['role.read', 'role.create']))
    await obsoleteLoad
    expect(auth.can('role.read')).toBe(true)
    expect(auth.can('role.create')).toBe(false)
    expect(auth.user?.role_authz_version).toBe(2)
    expect(request).toHaveBeenCalledTimes(2)
  },
)

it('coalesces permission reads within one authorization generation', async () => {
  setActivePinia(createPinia())
  const auth = useAuthStore()
  api.setSession(sessionFor())
  const response = pendingResponse()
  const request = vi.spyOn(api, 'request').mockImplementation(() => response.promise)
  const first = auth.loadPermissions()
  const second = auth.loadPermissions()
  expect(request).toHaveBeenCalledTimes(1)
  response.resolve(permissionsFor(['role.read']))
  await Promise.all([first, second])
  expect(request).toHaveBeenCalledTimes(1)
})

it.each(['resolve', 'reject'] as const)(
  'ignores a stale request %s after logout and a new login',
  async (outcome) => {
    setActivePinia(createPinia())
    const auth = useAuthStore()
    api.setSession(sessionFor())
    const old = pendingResponse()
    const request = vi
      .spyOn(api, 'request')
      .mockImplementationOnce(() => old.promise)
      .mockResolvedValueOnce(permissionsFor(['business.read'], 'other'))
    const obsoleteLoad = auth.loadPermissions()
    api.setSession(null)
    api.setSession(sessionFor(1, 'other'))
    await auth.loadPermissions()
    if (outcome === 'resolve') old.resolve(permissionsFor(['role.create']))
    else old.reject(new Error('old session failed'))
    await obsoleteLoad
    expect(auth.permissions?.user_id).toBe('other')
    expect(auth.can('role.create')).toBe(false)
    expect(request).toHaveBeenCalledTimes(2)
  },
)

it('does not restore permissions or issue a new read after logout', async () => {
  setActivePinia(createPinia())
  const auth = useAuthStore()
  api.setSession(sessionFor())
  const old = pendingResponse()
  const request = vi.spyOn(api, 'request').mockImplementationOnce(() => old.promise)
  const obsoleteLoad = auth.loadPermissions()
  api.setSession(null)
  old.resolve(permissionsFor(['role.create']))
  await obsoleteLoad
  expect(auth.permissions).toBeNull()
  expect(request).toHaveBeenCalledTimes(1)
})

it('retries permission loading after a current-generation request failure', async () => {
  setActivePinia(createPinia())
  const auth = useAuthStore()
  api.setSession(sessionFor())
  const request = vi
    .spyOn(api, 'request')
    .mockRejectedValueOnce(new Error('temporarily unavailable'))
    .mockResolvedValueOnce(permissionsFor(['role.read']))
  await expect(auth.loadPermissions()).rejects.toThrow('temporarily unavailable')
  await auth.loadPermissions()
  expect(auth.can('role.read')).toBe(true)
  expect(request).toHaveBeenCalledTimes(2)
})

it('waits for current permissions when the request itself refreshes the authorization generation', async () => {
  setActivePinia(createPinia())
  const auth = useAuthStore()
  api.setSession(sessionFor())
  const request = vi
    .spyOn(api, 'request')
    .mockImplementationOnce(async () => {
      api.setSession(sessionFor(2))
      return permissionsFor(['role.create'])
    })
    .mockResolvedValueOnce(permissionsFor(['role.read']))
  await auth.loadPermissions()
  expect(auth.user?.role_authz_version).toBe(2)
  expect(auth.can('role.read')).toBe(true)
  expect(auth.can('role.create')).toBe(false)
  expect(request).toHaveBeenCalledTimes(2)
})
