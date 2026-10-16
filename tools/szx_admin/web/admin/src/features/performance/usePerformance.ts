import { computed, onBeforeUnmount, onMounted, ref, shallowRef, watch } from 'vue'
import { useAuthStore } from '../../stores/auth'
import { systemApi } from '../../api/system'
import { ApiError, errorMessage } from '../../api/client'
import type { MachineSample, MachineSnapshot } from '../../api/types'
import { createMetricsPoller } from './poller'
import { mergeSamples } from './model'

export function usePerformance() {
  const auth = useAuthStore()
  const snapshot = shallowRef<MachineSnapshot>()
  const samples = shallowRef<MachineSample[]>([])
  const error = ref('')
  const paused = ref(false)
  const hidden = ref(document.hidden)
  const windowSeconds = ref(300)
  const now = ref(Date.now())
  let clock: ReturnType<typeof setInterval> | undefined
  let mounted = false
  const allowed = computed(() =>
    Boolean(auth.user && !auth.needsPassword && auth.can('system.metrics.read')),
  )
  const poller = createMetricsPoller(
    async (signal, current) => {
      const next = (await systemApi.snapshot(signal)).data
      if (!current()) return
      const previous = snapshot.value
      const restart = previous?.instance_id !== next.instance_id
      const gap = previous && next.sequence - previous.sequence > 1
      let history: MachineSample[] = []
      if (restart || gap) {
        const response = (await systemApi.history(signal)).data
        if (!current()) return
        // The server may restart between calls: retry, don't mix machine instances.
        if (response.instance_id !== next.instance_id) throw new ApiError(2216)
        history = response.samples
      }
      if (!current()) return
      samples.value = mergeSamples(restart ? [] : samples.value, [...history, next])
      snapshot.value = next
      now.value = Date.now()
      error.value = ''
    },
    (problem) => {
      error.value = errorMessage(problem)
    },
  )
  function synchronize() {
    clearInterval(clock)
    if (mounted && allowed.value && !paused.value && !hidden.value) {
      now.value = Date.now()
      clock = setInterval(() => {
        now.value = Date.now()
      }, 1000)
      poller.start()
    } else poller.stop()
    if (!allowed.value) {
      snapshot.value = undefined
      samples.value = []
    }
  }
  function visibility() {
    hidden.value = document.hidden
    synchronize()
  }
  watch([allowed, paused], synchronize)
  onMounted(() => {
    mounted = true
    document.addEventListener('visibilitychange', visibility)
    synchronize()
  })
  onBeforeUnmount(() => {
    mounted = false
    poller.stop()
    clearInterval(clock)
    document.removeEventListener('visibilitychange', visibility)
  })
  return {
    snapshot,
    samples,
    error,
    paused,
    hidden,
    windowSeconds,
    now,
    stale: computed(() =>
      Boolean(snapshot.value && now.value - snapshot.value.sampled_at > 10_000),
    ),
    visibleSamples: computed(() =>
      samples.value.filter(
        (item) =>
          item.sampled_at >=
          (samples.value.at(-1)?.sampled_at ?? now.value) - windowSeconds.value * 1000,
      ),
    ),
  }
}
