import { expect, it } from 'vitest'
import { visiblePages } from '../src/navigation'

it('orders management pages consistently for sidebar and home', () => {
  expect(visiblePages('super_admin', () => true).map((page) => page.to)).toEqual([
    '/home',
    '/users',
    '/roles',
    '/permissions',
    '/logs',
    '/system/performance',
  ])
})

it('only offers pages allowed by both capabilities and existing route role restrictions', () => {
  expect(visiblePages('operator', () => true).map((page) => page.to)).toEqual([
    '/home',
    '/roles',
    '/permissions',
  ])
  expect(
    visiblePages('admin', (capability) => capability === 'role.read').map((page) => page.to),
  ).toEqual(['/home', '/users', '/roles', '/logs'])
})
