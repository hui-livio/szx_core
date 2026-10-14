import { api } from './client'
import type { MachineHistory, MachineSnapshot } from './types'

export const systemApi = {
  snapshot: (signal: AbortSignal) =>
    api.request<MachineSnapshot>({ url: '/admin/system/metrics', signal }),
  history: (signal: AbortSignal) =>
    api.request<MachineHistory>({
      url: '/admin/system/metrics/history',
      params: { window_seconds: 900 },
      signal,
    }),
}
