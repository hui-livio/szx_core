import { computed, onUnmounted, shallowRef } from 'vue'
import type { Envelope, Page } from '../api/types'
import { errorMessage } from '../api/client'

export function usePagedQuery<T>(
  fetchPage: (page: number, size: number) => Promise<Envelope<Page<T>>>,
) {
  const items = shallowRef<T[]>([])
  const page = shallowRef(1)
  const pageSize = shallowRef(20)
  const total = shallowRef(0)
  const busy = shallowRef(false)
  const error = shallowRef('')
  const requestId = shallowRef('')
  const pageCount = computed(() => Math.max(1, Math.ceil(total.value / pageSize.value)))
  let sequence = 0
  function reset() {
    sequence++
    items.value = []
    page.value = 1
    total.value = 0
    busy.value = false
    error.value = ''
    requestId.value = ''
  }
  async function load(target = page.value) {
    const current = ++sequence
    busy.value = true
    error.value = ''
    items.value = []
    try {
      const response = await fetchPage(target, pageSize.value)
      if (current !== sequence) return
      items.value = response.data.items
      total.value = response.data.total
      page.value = response.data.page
      requestId.value = response.request_id
    } catch (failure) {
      if (current === sequence) error.value = errorMessage(failure)
    } finally {
      if (current === sequence) busy.value = false
    }
  }
  onUnmounted(() => {
    sequence++
  })
  return { items, page, pageSize, total, busy, error, requestId, pageCount, load, reset }
}
