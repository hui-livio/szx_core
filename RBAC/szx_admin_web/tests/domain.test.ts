import { expect, it } from 'vitest'
import {
  hashPassword,
  manageableRoles,
  passwordProblem,
  redactDetails,
  toUtcSeconds,
} from '../src/domain'
it('hashes exact UTF-8 input without trimming', async () => {
  expect(await hashPassword('abc')).toBe(
    'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad',
  )
  expect(await hashPassword(' abc ')).not.toBe(await hashPassword('abc'))
})
it('restricts manageable roles', () => {
  expect(manageableRoles('admin')).toEqual(['operator', 'engineer'])
  expect(manageableRoles('super_admin')).toHaveLength(4)
  expect(manageableRoles('operator')).toEqual([])
})
it('checks password strength, confirmation and reuse', () => {
  expect(passwordProblem('NewPassword123', 'old', 'NewPassword123')).toBe('')
  expect(passwordProblem('short1', 'old', 'short1')).not.toBe('')
  expect(passwordProblem('NewPassword123', 'NewPassword123', 'NewPassword123')).not.toBe('')
  expect(passwordProblem('NewPassword123', 'old', 'different')).not.toBe('')
})
it('removes secrets recursively from historical audit records', () => {
  expect(
    redactDetails({
      ok: 'visible',
      access_token: 'secret',
      nested: [{ private_key: 'secret', keep: 2 }],
    }),
  ).toEqual({ ok: 'visible', nested: [{ keep: 2 }] })
})
it('serializes audit query timestamps with second precision', () => {
  expect(toUtcSeconds('2026-09-17T09:15:00.123Z')).toBe('2026-09-17T09:15:00Z')
})
