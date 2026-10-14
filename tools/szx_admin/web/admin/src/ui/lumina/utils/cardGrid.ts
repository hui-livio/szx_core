/** Distribute cards evenly across the minimum number of rows that fit the container. */
export function balancedCardRows(count: number, capacity: number): number[] {
  const total = Number.isFinite(count) ? Math.max(0, Math.floor(count)) : 0
  if (!total) return []
  const columns = Math.min(total, Number.isFinite(capacity) ? Math.max(1, Math.floor(capacity)) : 1)
  const rows = Math.ceil(total / columns)
  const base = Math.floor(total / rows)
  const extra = total % rows
  return Array.from({ length: rows }, (_, index) => base + (index < extra ? 1 : 0))
}
