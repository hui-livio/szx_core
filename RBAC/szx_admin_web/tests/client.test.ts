import { describe, expect, it, vi } from 'vitest'
import axios, { AxiosError, type AxiosRequestConfig, type AxiosResponse } from 'axios'
import { createApiClient } from '../src/api/client'
import type { Session } from '../src/api/types'

const session = {
  access_token: 'old-access',
  csrf_token: 'old-csrf',
  expires_at: Date.now() + 90_000,
} as Session
function success(data: unknown, status = 200) {
  return { status, data: { data, request_id: 'test-request' }, headers: {} } as AxiosResponse
}
function failure(status: number, code: number) {
  return new AxiosError('failure', '', undefined, undefined, {
    status,
    data: { code, request_id: 'failed-request' },
  } as AxiosResponse)
}
function setup(handler: (config: AxiosRequestConfig) => Promise<AxiosResponse>) {
  const transport = axios.create()
  const request = vi.spyOn(transport, 'request').mockImplementation(handler)
  const client = createApiClient(transport)
  const changed = vi.fn()
  client.onSessionChange(changed)
  client.setSession(session)
  changed.mockClear()
  return { client, request, changed }
}
describe('session protocol', () => {
  it('coalesces simultaneous 401s and rotates both credentials before replay', async () => {
    const seen: AxiosRequestConfig[] = []
    const { client, request } = setup(async (config) => {
      seen.push(config)
      if (config.url === '/auth/refresh') {
        await new Promise((resolve) => setTimeout(resolve, 10))
        expect(config.headers?.['X-CSRF-Token']).toBe('old-csrf')
        return success({ ...session, access_token: 'new-access', csrf_token: 'new-csrf' })
      }
      if (config.headers?.Authorization === 'Bearer old-access') throw failure(401, 2201)
      return success({ ok: true })
    })
    await Promise.all([client.request({ url: '/a' }), client.request({ url: '/b' })])
    expect(request.mock.calls.filter(([config]) => config.url === '/auth/refresh')).toHaveLength(1)
    expect(
      seen.filter((config) => config.headers?.Authorization === 'Bearer new-access'),
    ).toHaveLength(2)
    expect(seen.every((config) => config.withCredentials)).toBe(true)
  })
  it.each([
    [401, 2200],
    [409, 2202],
    [403, 2204],
  ])('clears invalid refresh sessions: %s/%s', async (status, code) => {
    const { client, changed } = setup(async (config) => {
      throw config.url === '/auth/refresh' ? failure(status, code) : failure(401, 2201)
    })
    await expect(client.request({ url: '/users' })).rejects.toMatchObject({ code })
    expect(changed).toHaveBeenCalledWith(null)
  })
  it.each([
    [429, 2211],
    [503, 2216],
  ])('preserves session on temporary refresh failure: %s', async (status, code) => {
    const { client, changed, request } = setup(async (config) => {
      throw config.url === '/auth/refresh' ? failure(status, code) : failure(401, 2201)
    })
    await expect(client.request({ url: '/users' })).rejects.toMatchObject({ code })
    expect(changed).not.toHaveBeenCalled()
    expect(request).toHaveBeenCalledTimes(2)
  })
  it('does not treat a wrong current password as an expired session', async () => {
    const { client, request, changed } = setup(async () => {
      throw failure(401, 2203)
    })
    await expect(
      client.request({ url: '/auth/change-password', method: 'POST' }),
    ).rejects.toMatchObject({ code: 2203 })
    expect(request).toHaveBeenCalledTimes(1)
    expect(changed).not.toHaveBeenCalled()
  })
  it('does not replay conflicting writes and preserves If-Match', async () => {
    const { client, request } = setup(async (config) => {
      expect(config.headers?.['If-Match']).toBe('7')
      throw failure(409, 2209)
    })
    await expect(
      client.request({ url: '/admin/users/id', method: 'PATCH', headers: { 'If-Match': '7' } }),
    ).rejects.toMatchObject({ code: 2209 })
    expect(request).toHaveBeenCalledTimes(1)
  })
  it('clears only after successful 204 logout', async () => {
    const { client, changed } = setup(async (config) => {
      expect(config.headers?.['X-CSRF-Token']).toBe('old-csrf')
      return success(undefined, 204)
    })
    await client.logout()
    expect(changed).toHaveBeenCalledWith(null)
  })
  it('keeps the session on failed logout', async () => {
    const { client, changed } = setup(async () => {
      throw failure(500, 2212)
    })
    await expect(client.logout()).rejects.toMatchObject({ code: 2212 })
    expect(changed).not.toHaveBeenCalled()
  })
})
