export const MAX_RAW_BYTES = 1024 * 1024 * 1024
export const RANGE_BYTES = 1024 * 1024

/** Bind all raw requests to both the selected leaf name and an immutable snapshot id. */
export function imageUrl(path, name = '', id = '') {
  const params = new URLSearchParams()
  if (name) params.set('name', name)
  if (id) params.set('id', id)
  return params.size ? `${path}?${params}` : path
}

function wait(ms, signal) {
  return new Promise((resolve, reject) => {
    if (signal.aborted) {
      reject(new DOMException('Aborted', 'AbortError'))
      return
    }
    const abort = () => {
      clearTimeout(timer)
      reject(new DOMException('Aborted', 'AbortError'))
    }
    const timer = setTimeout(() => {
      signal.removeEventListener('abort', abort)
      resolve()
    }, ms)
    signal.addEventListener('abort', abort, { once: true })
  })
}
function failure(code, status) {
  const messages = {
    1002: '图片文件读取失败。',
    1003: '图片解码失败。',
    1004: '不支持该图片的像素格式。',
    1005: '图片超出服务端大小限制。',
    1007: '服务器内存不足。',
    1011: '图片名称无效。',
    1012: '图片已不存在，请刷新列表。',
    1015: '图片尚未加载，请先点击后端加载。',
    1018: '图片快照已变更，请重新加载。',
  }
  return new Error(messages[code] || `图片接口失败：HTTP ${status}`)
}

export function validateInfo(info) {
  if (
    info.state !== 'ready' ||
    typeof info.id !== 'string' ||
    !info.id ||
    info.format !== 'BGRA8' ||
    info.channels !== 4 ||
    info.depth_bits !== 8 ||
    info.origin !== 'top-left' ||
    info.alpha !== 'straight'
  )
    throw new Error('后端尚未完成 BGRA8 解码或像素格式无效。')
  validateShape(info.width, info.height, info.stride, info.byte_length)
  validateShape(
    info.preview_width,
    info.preview_height,
    info.preview_width * 4,
    info.preview_byte_length,
  )
  if (
    info.preview_width > 2048 ||
    info.preview_height > 2048 ||
    !Number.isInteger(info.tile_size) ||
    info.tile_size < 1 ||
    info.tile_size > 512 ||
    !Number.isInteger(info.max_level) ||
    info.max_level < 0 ||
    info.max_level > 20
  )
    throw new Error('预览/瓦片配置无效。')
  return info
}
function validateShape(width, height, stride, length) {
  if (
    !Number.isSafeInteger(width) ||
    !Number.isSafeInteger(height) ||
    width < 1 ||
    height < 1 ||
    stride !== width * 4 ||
    length !== stride * height ||
    !Number.isSafeInteger(length) ||
    length > MAX_RAW_BYTES
  )
    throw new Error('BGRA8 宽高、行步长或字节数无效。')
}

/** Only POST initiates read/decode/conversion; subsequent GETs just observe the same job. */
export async function loadBackend(name, signal) {
  const deadline = performance.now() + 120000
  let method = 'POST'
  for (;;) {
    const response = await fetch(imageUrl('/api/image/load', name), {
      method,
      signal,
      cache: 'no-store',
    })
    const info = await response.json()
    if (response.status === 200) return validateInfo(info)
    if (
      (response.status === 202 && info.code === 1013) ||
      (response.status === 429 && info.code === 1014)
    ) {
      method = response.status === 202 ? 'GET' : 'POST'
      if (performance.now() > deadline) throw new Error('后端加载超时，请重试。')
      await wait(300, signal)
      continue
    }
    throw failure(info.code, response.status)
  }
}

function rawHeaders(response, expectedId) {
  const headers = response.headers
  if (
    headers.get('content-type')?.split(';')[0] !== 'application/octet-stream' ||
    headers.get('x-pixel-format') !== 'BGRA8' ||
    headers.get('x-image-origin') !== 'top-left' ||
    headers.get('x-image-alpha') !== 'straight' ||
    headers.get('x-image-id') !== expectedId
  )
    throw new Error('BGRA8 响应格式或快照身份不匹配。')
  const shape = {
    width: Number(headers.get('x-image-width')),
    height: Number(headers.get('x-image-height')),
    stride: Number(headers.get('x-image-stride')),
    length: Number(headers.get('x-image-byte-length')),
  }
  validateShape(shape.width, shape.height, shape.stride, shape.length)
  return shape
}
async function assertOk(response, status) {
  if (response.status === status) return
  let code
  try {
    code = (await response.json()).code
  } catch {
    /* Invalid body remains an HTTP failure. */
  }
  throw failure(code, response.status)
}

/** One raw buffer. Large transfers write ranges directly into the final allocation, without PNG/Blob decoding. */
export async function rawImage(path, name, id, width, height, signal, progress) {
  const length = width * height * 4
  validateShape(width, height, width * 4, length)
  const url = imageUrl(path, name, id)
  let data
  if (length <= 16 * 1024 * 1024) {
    const response = await fetch(url, { signal, cache: 'no-store' })
    await assertOk(response, 200)
    const shape = rawHeaders(response, id)
    if (shape.width !== width || shape.height !== height) throw new Error('BGRA8 响应尺寸不匹配。')
    data = new Uint8Array(await response.arrayBuffer())
    if (data.byteLength !== length) throw new Error('BGRA8 数据长度不匹配。')
  } else {
    data = new Uint8Array(length)
    for (let first = 0; first < length; first += RANGE_BYTES) {
      const last = Math.min(length - 1, first + RANGE_BYTES - 1)
      const response = await fetch(url, {
        signal,
        cache: 'no-store',
        headers: { Range: `bytes=${first}-${last}` },
      })
      await assertOk(response, 206)
      const shape = rawHeaders(response, id)
      if (
        shape.width !== width ||
        shape.height !== height ||
        response.headers.get('content-range') !== `bytes ${first}-${last}/${length}`
      )
        throw new Error('BGRA8 分段范围不匹配。')
      const bytes = new Uint8Array(await response.arrayBuffer())
      if (bytes.length !== last - first + 1) throw new Error('BGRA8 分段长度不匹配。')
      data.set(bytes, first)
      progress?.((last + 1) / length)
    }
  }
  return { width, height, stride: width * 4, format: 'BGRA8', data }
}

export async function rawTile(path, name, id, signal) {
  const response = await fetch(imageUrl(path, name, id), { signal, cache: 'no-store' })
  await assertOk(response, 200)
  const shape = rawHeaders(response, id)
  if (shape.width > 512 || shape.height > 512) throw new Error('BGRA8 瓦片超出尺寸预算。')
  const data = new Uint8Array(await response.arrayBuffer())
  if (data.byteLength !== shape.length) throw new Error('BGRA8 瓦片长度不匹配。')
  return { ...shape, format: 'BGRA8', data }
}
