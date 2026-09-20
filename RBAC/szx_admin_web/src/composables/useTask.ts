import { shallowRef } from 'vue'
import { errorMessage } from '../api/client'

export function useTask() {
  const busy = shallowRef(false)
  const error = shallowRef('')
  async function run(action: () => Promise<void>) {
    if (busy.value) return
    busy.value = true
    error.value = ''
    try {
      await action()
    } catch (failure) {
      error.value = errorMessage(failure)
    } finally {
      busy.value = false
    }
  }
  return { busy, error, run }
}
