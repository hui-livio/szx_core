import { resolveFieldData } from '@primeuix/utils/object'
import { createUpdateSnapshot } from './updateSnapshot'

type DataKey<T> = PropertyKey | ((row: T) => unknown)
interface Entry<T> {
  row: T
  snapshot: ReturnType<typeof createUpdateSnapshot>
}

/** Keep the array and unchanged row identities stable across equivalent refreshes.
 * Rows remain caller-owned values; only private comparison snapshots are copied.
 */
export function createStableRows<T>() {
  let rows: T[] = []
  let entries: Entry<T>[] = []
  let lastKey: DataKey<T> | undefined
  return (incoming: readonly T[], dataKey?: DataKey<T>): T[] => {
    const keyOf = (row: T, index: number) =>
      typeof dataKey === 'function'
        ? dataKey(row)
        : typeof dataKey === 'string'
          ? resolveFieldData(row, dataKey)
          : dataKey !== undefined && typeof row === 'object' && row !== null
            ? Reflect.get(row, dataKey)
            : index
    const keyed = new Map<unknown, Entry<T>>()
    if (lastKey === dataKey)
      entries.forEach((entry, index) => keyed.set(keyOf(entry.row, index), entry))
    const used = new Set<Entry<T>>()
    const next = incoming.map((row, index) => {
      const previous = keyed.get(keyOf(row, index))
      if (previous && !used.has(previous)) {
        used.add(previous)
        if (previous.snapshot.matches(row)) return previous
      }
      const snapshot = createUpdateSnapshot()
      snapshot.commit(row)
      return { row, snapshot }
    })
    const unchanged =
      next.length === entries.length && next.every((entry, index) => entry === entries[index])
    entries = next
    lastKey = dataKey
    if (!unchanged) rows = next.map((entry) => entry.row)
    return rows
  }
}
