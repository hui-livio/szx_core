/** Normalize non-negative component lengths; CSS expressions pass through unchanged. */
export function toCssLength(value: number | string): string {
  return typeof value === 'number' ? `${Math.max(0, value)}px` : value
}
