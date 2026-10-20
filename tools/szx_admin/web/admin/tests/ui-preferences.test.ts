import { afterEach, expect, it, vi } from 'vitest'
import { createPinia } from 'pinia'
import { useUiPreferencesStore } from '../src/stores/uiPreferences'
import { resolveLuminaTheme } from '../src/ui/lumina/theme/appearance'

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

it('custom brand persists across store initialization without changing default colors', () => {
  const data = new Map<string, string>()
  vi.stubGlobal('localStorage', {
    getItem: (key: string) => data.get(key) ?? null,
    setItem: (key: string, value: string) => data.set(key, value),
  })
  const first = useUiPreferencesStore(createPinia())
  first.initialize()
  expect(first.brand).toBeNull()
  first.setBrand('#123ABC')
  expect(first.brand).toBe('#123abc')
  expect(data.get('szx-admin-brand')).toBe('#123abc')
  first.setBrand('invalid')
  expect(first.brand).toBe('#123abc')
  const restored = useUiPreferencesStore(createPinia())
  restored.initialize()
  expect(restored.brand).toBe('#123abc')
  restored.setBrand(null)
  const defaults = useUiPreferencesStore(createPinia())
  defaults.initialize()
  expect(defaults.brand).toBeNull()
})

it('restores the design mode and resolves a system theme change', () => {
  const data = new Map<string, string>([
    ['szx-admin-design', 'industrial'],
    ['szx-admin-theme', 'system'],
    ['szx-admin-effects', 'full'],
  ])
  let dark = false
  let listener: ((event: { matches: boolean }) => void) | undefined
  vi.stubGlobal('localStorage', {
    getItem: (key: string) => data.get(key) ?? null,
    setItem: (key: string, value: string) => data.set(key, value),
  })
  vi.stubGlobal('window', {
    matchMedia: () => ({
      get matches() {
        return dark
      },
      addEventListener: (_: string, update: (event: { matches: boolean }) => void) => {
        listener = update
      },
      removeEventListener: () => {},
    }),
  })
  const store = useUiPreferencesStore(createPinia())
  store.initialize()
  expect(store.design).toBe('industrial')
  expect(store.themePreference).toBe('system')
  expect(store.theme).toBe('light')
  dark = true
  listener?.({ matches: true })
  expect(store.theme).toBe('dark')
  expect(resolveLuminaTheme('system')).toBe('dark')
  store.setDesign('glass')
  expect(data.get('szx-admin-design')).toBe('glass')
})
