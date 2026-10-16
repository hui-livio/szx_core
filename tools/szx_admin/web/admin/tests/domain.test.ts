import { expect, it } from 'vitest'
import {
  hashPassword,
  formatDateTimeLocal,
  parseDateTimeLocal,
  manageableRoles,
  passwordProblem,
  redactDetails,
  roleLabel,
  toUtcSeconds,
} from '../src/domain'
it('displays unknown custom role codes without passing missing translation keys', () => {
  expect(roleLabel('custom_reviewer')).toBe('custom_reviewer')
})
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
it.each(['operator', 'engineer'] as const)('allows six simple characters for %s', (role) => {
  for (const password of ['123456', 'abcdef', '中文密码六个']) {
    expect(passwordProblem(password, 'old', password, role)).toBe('')
  }
  expect(passwordProblem('12345', 'old', '12345', role)).not.toBe('')
  expect(passwordProblem('123456', '123456', '123456', role)).not.toBe('')
  expect(passwordProblem('123456', 'old', '654321', role)).not.toBe('')
})
it.each(['admin', 'super_admin'] as const)('retains strong passwords for %s', (role) => {
  for (const password of ['123456', 'abcdef', 'Abc1234', '12345678', 'abcdefgh']) {
    expect(passwordProblem(password, 'old', password, role)).not.toBe('')
  }
  expect(passwordProblem('Password1234', 'old', 'Password1234', role)).toBe('')
  expect(passwordProblem('Abcd1234', 'old', 'Abcd1234', role)).toBe('')
  expect(passwordProblem('Abc1234!', 'old', 'Abc1234!', role)).toBe('')
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
it('serializes picker Dates as local wall time, retaining seconds', () => {
  const date = new Date(2026, 9, 13, 5, 6, 7)
  expect(formatDateTimeLocal(date)).toBe('2026-10-13T05:06:07')
  expect(toUtcSeconds(formatDateTimeLocal(date))).toBe(date.toISOString().replace('.000', ''))
  expect(formatDateTimeLocal(null)).toBe('')
  expect(formatDateTimeLocal(new Date('invalid'))).toBe('')
})

it.each([
  '2026-02-30T08:15:06',
  '2026-10-13T24:15:06',
  '2026-10-13T08:60:06',
  '2026-10-13T08:15:60',
  '2026-10-13',
  'invalid',
  '',
])('rejects invalid picker wall time: %s', (value) => {
  expect(parseDateTimeLocal(value)).toBeNull()
})
it('round trips valid leap-day local time without shifting zones', () => {
  const value = '2024-02-29T08:15:06'
  expect(formatDateTimeLocal(parseDateTimeLocal(value))).toBe(value)
})
