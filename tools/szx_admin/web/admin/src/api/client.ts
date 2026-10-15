import axios, { type AxiosInstance, type AxiosRequestConfig } from 'axios'
import type { Envelope, Session } from './types'
import { translate } from '../i18n'

const messages: Record<number, string> = {
  2200: 'errors.sessionInvalid',
  2201: 'errors.sessionExpired',
  2202: 'errors.sessionRevoked',
  2203: 'errors.invalidCredentials',
  2204: 'errors.forbidden',
  2205: 'errors.resourceNotFound',
  2206: 'errors.invalidInput',
  2207: 'errors.oneIdentifier',
  2208: 'errors.identifierUsed',
  2209: 'errors.conflict',
  2210: 'errors.lastSuperAdmin',
  2211: 'errors.rateLimited',
  2212: 'errors.auditWrite',
  2225: 'errors.behaviorQueue',
  2226: 'errors.behaviorWrite',
  2228: 'rbac.codeConflict',
  2227: 'roleManagement.codeConflict',
  2229: 'roleManagement.inUse',
  2231: 'roleManagement.protected',
  2230: 'rbac.inUse',
  2232: 'rbac.resourceProtected',
  2233: 'rbac.securityFloor',
  2215: 'errors.tooLarge',
  2216: 'errors.serviceUnavailable',
  2220: 'errors.databaseUnavailable',
}
export class ApiError extends Error {
  code: number
  requestId: string
  status: number
  constructor(code: number, requestId = '', status = 0) {
    super(translate(messages[code] ?? (status >= 500 ? 'errors.server' : 'errors.request')))
    this.name = 'ApiError'
    this.code = code
    this.requestId = requestId
    this.status = status
  }
}
export function errorMessage(error: unknown): string {
  if (error instanceof ApiError)
    return `${error.message}${error.code ? ` (${error.code})` : ''}${error.requestId ? ` · ${translate('common.requestId')}: ${error.requestId}` : ''}`
  return error instanceof Error ? error.message : translate('errors.operation')
}
function apiError(error: unknown): ApiError {
  if (error instanceof ApiError) return error
  if (axios.isAxiosError(error)) {
    const body = error.response?.data
    return new ApiError(
      typeof body?.code === 'number' ? body.code : 0,
      typeof body?.request_id === 'string' ? body.request_id : '',
      error.response?.status ?? 0,
    )
  }
  return new ApiError(0)
}

// Tokens remain in this closure, outside Pinia/devtools and browser storage.
export function createApiClient(
  transport: AxiosInstance = axios.create({
    baseURL: import.meta.env.VITE_API_BASE_URL || '/api/v1',
    withCredentials: true,
    timeout: 15_000,
  }),
) {
  let session: Session | null = null
  let refreshing: Promise<void> | null = null
  let generation = 0
  let sessionChanged: (value: Session | null) => void = () => {}
  function setSession(value: Session | null) {
    generation++
    session = value
    sessionChanged(value)
  }
  async function raw<T>(config: AxiosRequestConfig): Promise<Envelope<T>> {
    try {
      const response = await transport.request<Envelope<T>>({ ...config, withCredentials: true })
      if (response.status === 204)
        return { data: undefined as T, request_id: response.headers['x-request-id'] ?? '' }
      if (!response.data || !('data' in response.data)) throw new ApiError(0, '', response.status)
      return response.data
    } catch (error) {
      throw apiError(error)
    }
  }
  async function refresh() {
    if (refreshing) return refreshing
    if (!session) throw new ApiError(2200, '', 401)
    const current = generation
    const csrf = session.csrf_token
    refreshing = (async () => {
      try {
        const response = await raw<Session>({
          url: '/auth/refresh',
          method: 'POST',
          headers: { 'X-CSRF-Token': csrf },
        })
        if (current === generation) setSession(response.data)
      } catch (error) {
        const failure = apiError(error)
        if (
          current === generation &&
          (failure.status === 401 || failure.code === 2202 || failure.status === 403)
        )
          setSession(null)
        throw failure
      } finally {
        refreshing = null
      }
    })()
    return refreshing
  }
  async function request<T>(config: AxiosRequestConfig): Promise<Envelope<T>> {
    if (!session) throw new ApiError(2200, '', 401)
    if (session.expires_at <= Date.now() + 5_000) await refresh()
    if (!session) throw new ApiError(2200, '', 401)
    const token = session.access_token
    const send = () =>
      raw<T>({
        ...config,
        headers: { ...config.headers, Authorization: `Bearer ${session?.access_token}` },
      })
    try {
      return await send()
    } catch (error) {
      const failure = apiError(error)
      if (failure.status !== 401 || ![2200, 2201].includes(failure.code)) throw failure
      if (!session) throw failure
      if (session.access_token === token) await refresh()
      if (!session) throw failure
      try {
        return await send()
      } catch (retryError) {
        const retryFailure = apiError(retryError)
        if (retryFailure.status === 401 && [2200, 2201].includes(retryFailure.code))
          setSession(null)
        throw retryFailure
      }
    }
  }
  return {
    request,
    refresh,
    setSession,
    onSessionChange(callback: typeof sessionChanged) {
      sessionChanged = callback
    },
    async login(identifier: string, passwordHash: string) {
      if (refreshing) await refreshing
      const response = await raw<Session>({
        url: '/auth/login',
        method: 'POST',
        data: {
          login_identifier: identifier,
          password_hash: passwordHash,
        },
      })
      setSession(response.data)
    },
    async logout() {
      if (refreshing) await refreshing
      await raw<void>({
        url: '/auth/logout',
        method: 'POST',
        headers: { 'X-CSRF-Token': session?.csrf_token ?? '' },
      })
      setSession(null)
    },
  }
}
export const api = createApiClient()
