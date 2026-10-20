import { shallowRef } from 'vue'

const storageKey = 'szx-admin.metrics.hidden-cards'

function readHiddenCards(): string[] {
  try {
    const stored: unknown = JSON.parse(localStorage.getItem(storageKey) ?? '[]')
    return Array.isArray(stored)
      ? stored.filter((key): key is string => typeof key === 'string')
      : []
  } catch {
    return []
  }
}

export function usePerformanceCards() {
  const hiddenCards = shallowRef(readHiddenCards())
  const isVisible = (key: string) => !hiddenCards.value.includes(key)

  function toggleCard(key: string) {
    hiddenCards.value = isVisible(key)
      ? [...hiddenCards.value, key]
      : hiddenCards.value.filter((item) => item !== key)
    try {
      localStorage.setItem(storageKey, JSON.stringify(hiddenCards.value))
    } catch {
      // Keep this session's selection when browser storage is unavailable.
    }
  }

  return { hiddenCards, isVisible, toggleCard }
}
