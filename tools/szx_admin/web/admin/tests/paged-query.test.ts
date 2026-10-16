import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import type { Envelope, Page } from '../src/api/types'
import { parseLuminaPageSize } from '../src/ui/lumina/utils/pageSize'
import { usePagedQuery } from '../src/composables/usePagedQuery'

const lifecycle = vi.hoisted(() => ({ unmount: undefined as (() => void) | undefined }))
vi.mock('vue', async (importOriginal) => ({
  ...(await importOriginal<typeof import('vue')>()),
  onUnmounted: (callback: () => void) => {
    lifecycle.unmount = callback
  },
}))

beforeEach(() => {
  lifecycle.unmount = undefined
})
afterEach(() => vi.restoreAllMocks())

function response(page: number, size: number, item = `${page}:${size}`): Envelope<Page<string>> {
  return { data: { items: [item], total: 205, page, page_size: size }, request_id: item }
}
function deferred() {
  let resolve!: (value: Envelope<Page<string>>) => void
  const promise = new Promise<Envelope<Page<string>>>((done) => {
    resolve = done
  })
  return { promise, resolve }
}

it.each(['', ' ', '1.5', '1e2', '-1', '12px', 'Infinity', NaN, null, undefined, {}, true, 0, 101])(
  'rejects invalid or unsupported page size %s',
  (value) => {
    expect(parseLuminaPageSize(value, 1, 100)).toBeNull()
  },
)
it.each([1, 10, 20, 37, 50, 100, '37', ' 37 '])('accepts custom integer %s', (value) => {
  expect(parseLuminaPageSize(value, 1, 100)).toBe(Number(value))
})

describe('paged query size changes', () => {
  it('resets to page one and uses the selected size for subsequent loads and resets', async () => {
    const fetch = vi.fn(async (page: number, size: number) => response(page, size))
    const query = usePagedQuery(fetch)
    await query.load(3)
    expect(query.page.value).toBe(3)
    await query.setPageSize(37)
    expect(fetch).toHaveBeenLastCalledWith(1, 37)
    expect(query.page.value).toBe(1)
    expect(query.pageSize.value).toBe(37)
    expect(query.pageCount.value).toBe(6)
    await query.load(2)
    expect(fetch).toHaveBeenLastCalledWith(2, 37)
    query.reset()
    expect(query.page.value).toBe(1)
    expect(query.pageSize.value).toBe(37)
  })

  it.each([0, -1, 1.5, 101, Infinity, NaN, 20])(
    'does not request invalid or unchanged size %s',
    async (size) => {
      const fetch = vi.fn(async (page: number, size: number) => response(page, size))
      const query = usePagedQuery(fetch)
      await query.setPageSize(size)
      expect(fetch).not.toHaveBeenCalled()
      expect(query.pageSize.value).toBe(20)
    },
  )

  it('ignores an older page response after a size change', async () => {
    const old = deferred()
    const fetch = vi.fn((page: number, size: number) =>
      size === 20 ? old.promise : Promise.resolve(response(page, size)),
    )
    const query = usePagedQuery(fetch)
    const loading = query.load(3)
    await query.setPageSize(50)
    old.resolve(response(3, 20, 'stale'))
    await loading
    expect(query.items.value).toEqual(['1:50'])
    expect(query.page.value).toBe(1)
    expect(query.busy.value).toBe(false)
  })

  it('only publishes the latest of two concurrent size changes', async () => {
    const older = deferred()
    const newest = deferred()
    const query = usePagedQuery((_page, size) => (size === 10 ? older.promise : newest.promise))
    const loading = query.setPageSize(10)
    const next = query.setPageSize(37)
    older.resolve(response(1, 10))
    await loading
    expect(query.items.value).toEqual([])
    expect(query.busy.value).toBe(true)
    newest.resolve(response(1, 37))
    await next
    expect(query.items.value).toEqual(['1:37'])
    expect(query.pageSize.value).toBe(37)
  })

  it('retains the selected size and first page for retry after request failure', async () => {
    const fetch = vi.fn(async (page: number, size: number) => response(page, size))
    const query = usePagedQuery(fetch)
    await query.load(3)
    fetch.mockRejectedValueOnce(new Error('request failed'))
    await query.setPageSize(50)
    expect(query.error.value).not.toBe('')
    expect(query.items.value).toEqual([])
    expect(query.busy.value).toBe(false)
    expect(query.page.value).toBe(1)
    await query.load()
    expect(fetch).toHaveBeenLastCalledWith(1, 50)
    expect(query.items.value).toEqual(['1:50'])
    expect(query.error.value).toBe('')
  })

  it('does not publish an in-flight size change after unmount', async () => {
    const pending = deferred()
    const query = usePagedQuery(() => pending.promise)
    const loading = query.setPageSize(100)
    lifecycle.unmount!()
    pending.resolve(response(1, 100))
    await loading
    expect(query.items.value).toEqual([])
  })
})
