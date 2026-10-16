/** Parse a complete positive integer; partial input, decimals and exponents are invalid. */
export function parseLuminaPageSize(
  value: unknown,
  minimum = 1,
  maximum = Number.MAX_SAFE_INTEGER,
): number | null {
  if (typeof value !== 'number' && typeof value !== 'string') return null
  if (typeof value === 'string' && !/^\d+$/.test(value.trim())) return null
  const size = Number(value)
  return Number.isSafeInteger(size) && size >= minimum && size <= maximum ? size : null
}
