import { afterEach, beforeEach, expect, it, vi } from 'vitest'
import { registerLuminaDialog, topLuminaDialog } from '../../src/ui/lumina/utils/dialogStack'

class TestNode extends EventTarget {
  querySelectorAll() {
    return []
  }
}

const cleanups: (() => void)[] = []
let documentTarget: TestNode
beforeEach(() => {
  documentTarget = new TestNode()
  vi.stubGlobal('document', documentTarget)
  vi.stubGlobal('Node', TestNode)
})
afterEach(() => {
  while (cleanups.length) cleanups.pop()!()
  vi.restoreAllMocks()
  vi.unstubAllGlobals()
})

function register(close = vi.fn()) {
  const root = new TestNode() as unknown as HTMLElement
  const remove = registerLuminaDialog({ element: () => root, requestClose: close })
  cleanups.push(remove)
  return { root, close, remove }
}

function key(overrides = {}) {
  return Object.assign(new Event('keydown', { cancelable: true }), {
    key: 'Escape',
    code: 'Escape',
    isComposing: false,
    repeat: false,
    ...overrides,
  })
}

it('uses one capture listener and removes it after the final registration', () => {
  const add = vi.spyOn(documentTarget, 'addEventListener')
  const remove = vi.spyOn(documentTarget, 'removeEventListener')
  const parent = register()
  const child = register()
  expect(add).toHaveBeenCalledExactlyOnceWith('keydown', expect.any(Function), true)
  expect(topLuminaDialog()).toBe(child.root)
  parent.remove()
  expect(topLuminaDialog()).toBe(child.root)
  expect(remove).not.toHaveBeenCalled()
  child.remove()
  child.remove()
  expect(remove).toHaveBeenCalledTimes(1)
  expect(topLuminaDialog()).toBeNull()
})

it('only asks the top dialog to close and respects a caller that vetoes closing', () => {
  const parent = register()
  const child = register()
  documentTarget.dispatchEvent(key())
  documentTarget.dispatchEvent(key())
  expect(child.close).toHaveBeenCalledTimes(2)
  expect(parent.close).not.toHaveBeenCalled()
  child.remove()
  documentTarget.dispatchEvent(key())
  expect(parent.close).toHaveBeenCalledTimes(1)
})

it('ignores composition, held keys, and non-Escape keys', () => {
  const dialog = register()
  for (const event of [
    key({ isComposing: true }),
    key({ repeat: true }),
    key({ key: 'Enter', code: 'Enter' }),
  ]) {
    documentTarget.dispatchEvent(event)
    expect(event.defaultPrevented).toBe(false)
  }
  expect(dialog.close).not.toHaveBeenCalled()
  const escape = key()
  documentTarget.dispatchEvent(escape)
  expect(escape.defaultPrevented).toBe(true)
  expect(dialog.close).toHaveBeenCalledTimes(1)
})
