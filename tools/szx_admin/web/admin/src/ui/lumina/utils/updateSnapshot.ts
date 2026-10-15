// Render data is normally a tree of plain objects/arrays. Retain callbacks and
// opaque instances by identity rather than serializing them or losing semantics.
type Snapshot =
  | { kind: 'value'; value: unknown }
  | { kind: 'date'; value: number }
  | { kind: 'bytes'; constructor: unknown; value: Uint8Array }
  | { kind: 'fields'; prototype: object | null; fields: Map<PropertyKey, Snapshot> }

function fieldsOf(value: object): PropertyKey[] {
  return Reflect.ownKeys(value).filter((key) =>
    Object.prototype.propertyIsEnumerable.call(value, key),
  )
}

function bytesOf(value: unknown): Uint8Array | undefined {
  if (value instanceof ArrayBuffer) return new Uint8Array(value)
  if (ArrayBuffer.isView(value))
    return new Uint8Array(value.buffer, value.byteOffset, value.byteLength)
  return undefined
}

function capture(value: unknown, seen = new WeakMap<object, Snapshot>()): Snapshot {
  if (typeof value !== 'object' || value === null) return { kind: 'value', value }
  const previous = seen.get(value)
  if (previous) return previous
  if (value instanceof Date) return { kind: 'date', value: value.getTime() }
  const bytes = bytesOf(value)
  if (bytes) return { kind: 'bytes', constructor: value.constructor, value: bytes.slice() }
  const prototype: object | null = Object.getPrototypeOf(value)
  if (!Array.isArray(value) && prototype !== Object.prototype && prototype !== null)
    return { kind: 'value', value }
  const fields = new Map<PropertyKey, Snapshot>()
  const snapshot: Snapshot = { kind: 'fields', prototype, fields }
  seen.set(value, snapshot)
  // Length distinguishes empty/sparse arrays and detects truncation.
  if (Array.isArray(value)) fields.set('length', { kind: 'value', value: value.length })
  for (const key of fieldsOf(value)) fields.set(key, capture(Reflect.get(value, key), seen))
  return snapshot
}

function matches(snapshot: Snapshot, value: unknown, seen = new Map<Snapshot, object>()): boolean {
  if (snapshot.kind === 'value') return Object.is(snapshot.value, value)
  if (snapshot.kind === 'date')
    return value instanceof Date && Object.is(snapshot.value, value.getTime())
  if (snapshot.kind === 'bytes') {
    const bytes = bytesOf(value)
    return (
      bytes !== undefined &&
      value?.constructor === snapshot.constructor &&
      bytes.length === snapshot.value.length &&
      bytes.every((byte, index) => byte === snapshot.value[index])
    )
  }
  if (
    typeof value !== 'object' ||
    value === null ||
    Object.getPrototypeOf(value) !== snapshot.prototype
  )
    return false
  if (seen.has(snapshot)) return seen.get(snapshot) === value
  seen.set(snapshot, value)
  const keys = fieldsOf(value)
  if (Array.isArray(value)) keys.push('length')
  return (
    keys.length === snapshot.fields.size &&
    keys.every((key) => {
      const field = snapshot.fields.get(key)
      return field !== undefined && matches(field, Reflect.get(value, key), seen)
    })
  )
}

/** Compare against the last committed data, including later in-place mutations.
 * Plain objects, arrays, dates and buffers compare by content; callbacks and
 * other instances compare by identity. Replace opaque instances to update them.
 */
export function createUpdateSnapshot() {
  let snapshot: Snapshot | undefined
  return {
    matches(value: unknown): boolean {
      return snapshot !== undefined && matches(snapshot, value)
    },
    commit(value: unknown) {
      snapshot = capture(value)
    },
  }
}
