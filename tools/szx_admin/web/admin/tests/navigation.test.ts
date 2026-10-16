import { expect, it } from 'vitest'
import { hasPageAccess, visiblePages } from '../src/navigation'

it('orders management pages consistently for sidebar and home', () => {
  expect(visiblePages(() => true).map((page) => page.to)).toEqual([
    '/home',
    '/users',
    '/roles',
    '/permissions',
    '/logs',
    '/system/performance',
  ])
})

it.each(['audit.read', 'behavior.read'])(
  'offers the log page for independent %s rights',
  (capability) => {
    const can = (value: string) => value === capability
    expect(visiblePages(can).map((page) => page.to)).toEqual(['/home', '/logs'])
    expect(hasPageAccess({ capabilityAny: ['audit.read', 'behavior.read'] }, can)).toBe(true)
  },
)

it('denies pages when none of their alternative capabilities is present', () => {
  expect(hasPageAccess({ capabilityAny: ['audit.read', 'behavior.read'] }, () => false)).toBe(false)
  expect(hasPageAccess({ capabilityAny: [] }, () => true)).toBe(false)
})

it('uses capabilities rather than fixed role names to offer management pages', () => {
  expect(visiblePages((capability) => capability === 'role.read').map((page) => page.to)).toEqual([
    '/home',
    '/roles',
  ])
  expect(visiblePages((capability) => capability === 'user.read').map((page) => page.to)).toEqual([
    '/home',
    '/users',
  ])
})
