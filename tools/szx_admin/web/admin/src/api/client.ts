import axios, { type AxiosInstance, type AxiosRequestConfig } from 'axios'
import type { Envelope, Session } from './types'

const messages: Record<number, string> = {
  2200: '会话已失效，请重新登录。',
  2201: '会话已过期，请重新登录。',
  2202: '会话已被撤销，请重新登录。',
  2203: '账号或密码不正确，或账号当前不可用。',
  2204: '没有执行此操作的权限。',
  2205: '用户不存在，请重新加载列表。',
  2206: '输入不符合要求，请检查后重试。',
  2207: '请只提供一种登录标识。',
  2208: '登录标识已被使用，请更换。',
  2209: '数据已被其他请求修改，请重新加载后确认并提交。',
  2210: '不能禁用或降级最后一位启用的超级管理员。',
  2211: '请求过于频繁，请稍后手动重试。',
  2212: '审计记录写入失败，本次操作未完成。',
  2225: '行为事件接收队列已满，请稍后重试。',
  2226: '行为事件写入暂不可用，请稍后重试。',
  2215: '提交的内容过大。',
  2216: '服务暂不可用，请稍后重试。',
  2220: '数据库暂不可用，请稍后重试。',
}
export class ApiError extends Error {
  code: number
  requestId: string
  status: number
  constructor(code: number, requestId = '', status = 0) {
    super(
      messages[code] ??
        (status >= 500 ? '服务处理失败，请稍后重试。' : '请求失败，请检查网络或服务配置后重试。'),
    )
    this.name = 'ApiError'
    this.code = code
    this.requestId = requestId
    this.status = status
  }
}
export function errorMessage(error: unknown): string {
  if (error instanceof ApiError)
    return `${error.message}${error.code ? `（${error.code}）` : ''}${error.requestId ? ` · 请求 ID：${error.requestId}` : ''}`
  return error instanceof Error ? error.message : '操作未完成，请重试。'
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
