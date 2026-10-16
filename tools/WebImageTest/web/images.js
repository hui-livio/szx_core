export const MAX_FULL_IMAGE_BYTES = 512 * 1024 * 1024

/** All image endpoints use the same encoded leaf name, including tiles. */
export function imageUrl(path, name = '') {
  return name ? `${path}?${new URLSearchParams({ name })}` : path
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

export async function metadata(name, signal) {
  const deadline = performance.now() + 120000
  let first = true
  for (;;) {
    const url = imageUrl('/api/image/info', name) + (first ? `${name ? '&' : '?'}retry=1` : '')
    const response = await fetch(url, { signal, cache: 'no-store' })
    const info = await response.json()
    if (response.status !== 429) first = false
    if (
      (response.status === 202 && info.code === 1013) ||
      (response.status === 429 && info.code === 1014)
    ) {
      if (performance.now() > deadline) throw new Error('图片加载超时，请稍后重试。')
      await wait(500, signal)
      continue
    }
    if (response.status !== 200) {
      const messages = {
        1002: '图片文件读取失败。',
        1003: '图片解码失败。',
        1004: '不支持该图片的像素格式。',
        1005: '图片超出服务端大小限制。',
        1007: '服务器内存不足。',
        1011: '图片名称无效。',
        1012: '图片已不存在，请刷新列表。',
        1015: '图片缓存已切换，请重新获取图片。',
      }
      throw new Error(messages[info.code] || `图片加载失败：HTTP ${response.status}`)
    }
    return info
  }
}

/** Full PNG transport in bounded HTTP byte ranges, not spatial image tiles. */
export async function fullImage(name, encodedBytes, signal, progress) {
  if (
    !Number.isSafeInteger(encodedBytes) ||
    encodedBytes < 1 ||
    encodedBytes > MAX_FULL_IMAGE_BYTES
  )
    throw new Error('整图编码大小无效或超过 512 MiB。')
  const size = 1024 * 1024
  const parts = []
  for (let offset = 0; offset < encodedBytes; offset += size) {
    const end = Math.min(encodedBytes - 1, offset + size - 1)
    const response = await fetch(imageUrl('/api/image', name), {
      signal,
      cache: 'no-store',
      headers: { Range: `bytes=${offset}-${end}` },
    })
    if (
      response.status !== 206 ||
      response.headers.get('content-range') !== `bytes ${offset}-${end}/${encodedBytes}` ||
      response.headers.get('content-type')?.split(';')[0] !== 'image/png'
    )
      throw new Error(`整图分段下载失败：HTTP ${response.status}`)
    const part = await response.blob()
    if (part.size !== end - offset + 1) throw new Error('整图分段长度不匹配。')
    parts.push(part)
    progress?.((end + 1) / encodedBytes)
  }
  return new Blob(parts, { type: 'image/png' })
}
