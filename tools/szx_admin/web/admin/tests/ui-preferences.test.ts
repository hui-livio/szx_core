import { afterEach, expect, it, vi } from 'vitest'
import { createPinia } from 'pinia'
import { useUiPreferencesStore } from '../src/stores/uiPreferences'

afterEach(() => vi.unstubAllGlobals())

it('all consumers share the same initialized appearance state', () => {
  const data = new Map([
    ['szx-admin-theme', 'dark'],
    ['szx-admin-effects', 'full'],
  ])
  vi.stubGlobal('localStorage', {
    getItem: (key: string) => data.get(key) ?? null,
    setItem: (key: string, value: string) => data.set(key, value),
  })
  const pinia = createPinia()
  const login = useUiPreferencesStore(pinia)
  login.initialize()
  const shell = useUiPreferencesStore(pinia)
  expect(shell).toBe(login)
  expect(shell.theme).toBe('dark')
  expect(shell.effects).toBe('full')
  shell.setEffects('lite')
  expect(login.effects).toBe('lite')
  login.setTheme('light')
  expect(shell.theme).toBe('light')
  // A second initialization must not rehydrate over the current shared state.
  data.set('szx-admin-theme', 'dark')
  shell.initialize()
  expect(shell.theme).toBe('light')
})

it('blocked storage does not prevent shared in-memory preference updates', () => {
  vi.stubGlobal('localStorage', {
    getItem: () => {
      throw new Error('blocked')
    },
    setItem: () => {
      throw new Error('blocked')
    },
  })
  const store = useUiPreferencesStore(createPinia())
  store.initialize()
  store.setTheme('dark')
  store.setEffects('full')
  expect(store.theme).toBe('dark')
  expect(store.effects).toBe('full')
})
