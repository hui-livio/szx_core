import { describe, expect, it } from 'vitest'
import { createUpdateSnapshot } from '../../src/ui/lumina/utils/updateSnapshot'
import { createStableRows } from '../../src/ui/lumina/utils/stableRows'

describe('render update snapshots', () => {
  it('compares equivalent plain data without depending on object key order', () => {
    const snapshot = createUpdateSnapshot()
    expect(snapshot.matches(undefined)).toBe(false)
    snapshot.commit({ name: 'A', data: [1, NaN, undefined, null], nested: { enabled: true } })
    expect(
      snapshot.matches({ nested: { enabled: true }, data: [1, NaN, undefined, null], name: 'A' }),
    ).toBe(true)
    expect(
      snapshot.matches({ name: 'A', data: [1, NaN, null, null], nested: { enabled: true } }),
    ).toBe(false)
    expect(
      snapshot.matches({ name: 'A', data: [1, NaN, undefined, null], nested: { enabled: false } }),
    ).toBe(false)
  })
  it('detects in-place mutation, deletions, and array reordering', () => {
    const snapshot = createUpdateSnapshot()
    const data = { series: [{ data: [1, 2] }], title: 'Chart' }
    snapshot.commit(data)
    data.series[0]!.data[1] = 3
    expect(snapshot.matches(data)).toBe(false)
    snapshot.commit(data)
    expect(snapshot.matches(data)).toBe(true)
    data.series[0]!.data.reverse()
    expect(snapshot.matches(data)).toBe(false)
    expect(snapshot.matches({ series: [{ data: [1, 3] }] })).toBe(false)
  })
  it('retains callback identity rather than stripping functions like JSON serialization', () => {
    const snapshot = createUpdateSnapshot()
    const formatter = (n: number) => `${n}%`
    snapshot.commit({ formatter })
    expect(snapshot.matches({ formatter })).toBe(true)
    expect(snapshot.matches({ formatter: (n: number) => `${n}%` })).toBe(false)
  })
  it('handles dates, typed buffers and their mutations', () => {
    const snapshot = createUpdateSnapshot()
    const date = new Date(1000)
    const buffer = new Float32Array([1, 2])
    snapshot.commit({ date, buffer })
    expect(snapshot.matches({ date: new Date(1000), buffer: new Float32Array([1, 2]) })).toBe(true)
    date.setTime(2000)
    expect(snapshot.matches({ date, buffer })).toBe(false)
    snapshot.commit({ date, buffer })
    buffer[1] = 4
    expect(snapshot.matches({ date, buffer })).toBe(false)
  })
  it('handles cycles, sparse arrays, symbols and opaque instances conservatively', () => {
    const snapshot = createUpdateSnapshot()
    const a: { value: number; self?: unknown } = { value: 1 }
    a.self = a
    const b: { value: number; self?: unknown } = { value: 1 }
    b.self = b
    snapshot.commit(a)
    expect(snapshot.matches(b)).toBe(true)
    b.value = 2
    expect(snapshot.matches(b)).toBe(false)
    snapshot.commit(Array(2))
    expect(snapshot.matches([])).toBe(false)
    expect(snapshot.matches([undefined, undefined])).toBe(false)
    const key = Symbol('key')
    snapshot.commit({ [key]: 1 })
    expect(snapshot.matches({ [key]: 2 })).toBe(false)
    class Custom {
      value = 1
    }
    const custom = new Custom()
    snapshot.commit(custom)
    expect(snapshot.matches(custom)).toBe(true)
    expect(snapshot.matches(new Custom())).toBe(false)
  })
})

describe('stable table rows', () => {
  it('keeps the array and unchanged row identity on repeated equivalent responses', () => {
    const stabilize = createStableRows<{ id: string; nested: { value: number } }>()
    const first = stabilize(
      [
        { id: 'a', nested: { value: 1 } },
        { id: 'b', nested: { value: 2 } },
      ],
      'id',
    )
    const equal = stabilize(
      [
        { id: 'a', nested: { value: 1 } },
        { id: 'b', nested: { value: 2 } },
      ],
      'id',
    )
    expect(equal).toBe(first)
    const changed = stabilize(
      [
        { id: 'a', nested: { value: 1 } },
        { id: 'b', nested: { value: 3 } },
      ],
      'id',
    )
    expect(changed).not.toBe(first)
    expect(changed[0]).toBe(first[0])
    expect(changed[1]).not.toBe(first[1])
  })
  it('detects in-place changes and preserves order, insertion, removal and empty states', () => {
    const stabilize = createStableRows<{ id: string; value: number }>()
    const data = [
      { id: 'a', value: 1 },
      { id: 'b', value: 2 },
    ]
    const first = stabilize(data, 'id')
    data[1]!.value = 3
    const mutated = stabilize(data, 'id')
    expect(mutated).not.toBe(first)
    expect(mutated[1]!.value).toBe(3)
    const reordered = stabilize([data[1]!, data[0]!], 'id')
    expect(reordered[0]).toBe(mutated[1])
    expect(stabilize([{ id: 'c', value: 4 }, ...data], 'id').map((row) => row.id)).toEqual([
      'c',
      'a',
      'b',
    ])
    expect(stabilize([], 'id')).toEqual([])
    const empty = stabilize([], 'id')
    expect(stabilize([], 'id')).toBe(empty)
  })
  it('supports key functions, nested keys, index fallback and changing the key contract', () => {
    const stabilize = createStableRows<{ key: { id: string }; value: number }>()
    const key = (row: { key: { id: string } }) => row.key.id
    const first = stabilize([{ key: { id: 'a' }, value: 1 }], key)
    expect(stabilize([{ key: { id: 'a' }, value: 1 }], key)).toBe(first)
    expect(stabilize([{ key: { id: 'a' }, value: 1 }], 'key.id')).not.toBe(first)
    const indexed = stabilize([{ key: { id: 'a' }, value: 1 }])
    expect(stabilize([{ key: { id: 'a' }, value: 1 }])).toBe(indexed)
  })
  it('retains Vue dependency tracking for fields changed in place', async () => {
    const { computed, reactive } = await import('vue')
    const input = reactive([{ id: 'a', data: { value: 1 } }])
    const stabilize = createStableRows<(typeof input)[number]>()
    const rows = computed(() => stabilize(input, 'id'))
    const first = rows.value
    input[0]!.data.value = 2
    expect(rows.value).not.toBe(first)
    expect(rows.value[0]!.data.value).toBe(2)
  })
})

it('equivalent table data does not notify downstream value watchers', async () => {
  const { computed, ref, watch } = await import('vue')
  const input = ref([{ id: 'a', value: 1 }])
  const stabilize = createStableRows<(typeof input.value)[number]>()
  const rows = computed(() => stabilize(input.value, 'id'))
  let updates = 0
  const stop = watch(
    rows,
    () => {
      updates++
    },
    { flush: 'sync' },
  )
  input.value = [{ id: 'a', value: 1 }]
  input.value = [{ id: 'a', value: 1 }]
  expect(updates).toBe(0)
  input.value = [{ id: 'a', value: 2 }]
  expect(updates).toBe(1)
  stop()
})

it('table keys support numeric and symbol fields without conflating row identities', () => {
  const key = Symbol('id')
  const rows = createStableRows<{ [key]: string; 0: string; value: number }>()
  const data = [{ [key]: 'a', 0: 'a', value: 1 }]
  const numeric = rows(data, 0)
  expect(rows([{ [key]: 'a', 0: 'a', value: 1 }], 0)).toBe(numeric)
  const symbolic = rows(data, key)
  expect(rows([{ [key]: 'a', 0: 'a', value: 1 }], key)).toBe(symbolic)
})
