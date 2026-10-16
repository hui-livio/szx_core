import test from 'node:test'
import assert from 'node:assert/strict'
import {
  imageUrl,
  loadBackend,
  rawImage,
  rawTile,
  validateInfo,
  MAX_RAW_BYTES,
} from '../web/images.js'

const descriptor = (width = 3, height = 2) => ({
  state: 'ready',
  id: 'snapshot-a',
  width,
  height,
  format: 'BGRA8',
  stride: width * 4,
  byte_length: width * height * 4,
  channels: 4,
  depth_bits: 8,
  origin: 'top-left',
  alpha: 'straight',
  preview_width: width,
  preview_height: height,
  preview_byte_length: width * height * 4,
  tile_size: 512,
  max_level: 0,
  tiled: false,
})
const headers = (width, height, id = 'snapshot-a') => ({
  'content-type': 'application/octet-stream',
  'x-pixel-format': 'BGRA8',
  'x-image-width': String(width),
  'x-image-height': String(height),
  'x-image-stride': String(width * 4),
  'x-image-byte-length': String(width * height * 4),
  'x-image-origin': 'top-left',
  'x-image-alpha': 'straight',
  'x-image-id': id,
})

test('raw metadata requires completed BGRA8 conversion, packed rows and a snapshot id', () => {
  assert.equal(validateInfo(descriptor()).format, 'BGRA8')
  for (const patch of [
    { state: 'loading' },
    { format: 'BGR8' },
    { channels: 3 },
    { stride: 13 },
    { byte_length: 25 },
    { id: '' },
    { origin: 'bottom-left' },
    { alpha: 'premultiplied' },
    { width: NaN },
    { preview_width: 4096 },
  ])
    assert.throws(() => validateInfo({ ...descriptor(), ...patch }))
})
test('all raw endpoints carry URL encoded filename and immutable id', () => {
  for (const path of ['/api/image', '/api/image/preview', '/api/image/tile/0/1/2']) {
    const url = new URL(imageUrl(path, '蓝色 图片 #2.PPM', 'id/a'), 'http://localhost')
    assert.equal(url.searchParams.get('name'), '蓝色 图片 #2.PPM')
    assert.equal(url.searchParams.get('id'), 'id/a')
    assert.equal(url.hash, '')
    assert.equal(url.pathname, path)
  }
})
test('load uses POST once, then GET status until BGRA8 is ready', async () => {
  const old = globalThis.fetch
  const methods = []
  globalThis.fetch = async (url, options) => {
    assert.equal(new URL(url, 'http://localhost').pathname, '/api/image/load')
    methods.push(options.method)
    return methods.length === 1
      ? new Response(JSON.stringify({ code: 1013 }), { status: 202 })
      : new Response(JSON.stringify(descriptor()))
  }
  try {
    const info = await loadBackend('a.png', new AbortController().signal)
    assert.equal(info.format, 'BGRA8')
    assert.deepEqual(methods, ['POST', 'GET'])
  } finally {
    globalThis.fetch = old
  }
})
test('terminal decode failure is not silently retried', async () => {
  const old = globalThis.fetch
  let calls = 0
  globalThis.fetch = async () => {
    calls++
    return new Response(JSON.stringify({ code: 1003 }), { status: 422 })
  }
  try {
    await assert.rejects(loadBackend('bad.png', new AbortController().signal), /解码失败/)
    assert.equal(calls, 1)
  } finally {
    globalThis.fetch = old
  }
})
test('selection changes abort load polling without starting data transfer', async () => {
  const old = globalThis.fetch
  const controller = new AbortController()
  globalThis.fetch = async () => new Response(JSON.stringify({ code: 1013 }), { status: 202 })
  try {
    const work = loadBackend('a.png', controller.signal)
    setImmediate(() => controller.abort())
    await assert.rejects(work, (error) => error.name === 'AbortError')
  } finally {
    globalThis.fetch = old
  }
})
test('small image and tile preserve exact BGRA8 bytes, including alpha and channel order', async () => {
  const old = globalThis.fetch
  const bytes = new Uint8Array([17, 83, 211, 137, 0, 255, 7, 255])
  globalThis.fetch = async () => new Response(bytes, { headers: headers(2, 1) })
  try {
    const image = await rawImage(
      '/api/image',
      'a.png',
      'snapshot-a',
      2,
      1,
      new AbortController().signal,
    )
    assert.equal(image.format, 'BGRA8')
    assert.deepEqual(image.data, bytes)
    const tile = await rawTile(
      '/api/image/tile/0/0/0',
      'a.png',
      'snapshot-a',
      new AbortController().signal,
    )
    assert.deepEqual(tile.data, bytes)
    assert.equal(tile.stride, 8)
  } finally {
    globalThis.fetch = old
  }
})
test('large raw image ranges fill a single final typed array with exact length and identity', async () => {
  const old = globalThis.fetch
  const width = 2049,
    height = 2048,
    length = width * height * 4
  const ranges = []
  globalThis.fetch = async (url, options) => {
    const [, a, b] = options.headers.Range.match(/^bytes=(\d+)-(\d+)$/)
    const first = Number(a),
      last = Number(b)
    assert.equal(new URL(url, 'http://localhost').searchParams.get('id'), 'snapshot-a')
    assert.ok(last - first < 1024 * 1024)
    ranges.push([first, last])
    const bytes = new Uint8Array(last - first + 1)
    bytes.fill(37)
    if (last === length - 1) bytes[bytes.length - 1] = 93
    return new Response(bytes, {
      status: 206,
      headers: { ...headers(width, height), 'content-range': `bytes ${first}-${last}/${length}` },
    })
  }
  try {
    const image = await rawImage(
      '/api/image',
      'a.png',
      'snapshot-a',
      width,
      height,
      new AbortController().signal,
    )
    assert.equal(image.data.length, length)
    assert.equal(image.data[0], 37)
    assert.equal(image.data[length - 1], 93)
    assert.deepEqual(ranges.at(-1), [16 * 1024 * 1024, length - 1])
    assert.equal(ranges.length, 17)
  } finally {
    globalThis.fetch = old
  }
})
test('incorrect format, layout, stale id or truncated raw bytes are rejected', async () => {
  const old = globalThis.fetch
  try {
    for (const patch of [
      { 'content-type': 'image/png' },
      { 'x-pixel-format': 'RGBA8' },
      { 'x-image-id': 'old' },
      { 'x-image-stride': '99' },
    ]) {
      globalThis.fetch = async () =>
        new Response(new Uint8Array(8), { headers: { ...headers(2, 1), ...patch } })
      await assert.rejects(
        rawImage('/api/image', 'a.png', 'snapshot-a', 2, 1, new AbortController().signal),
      )
    }
    globalThis.fetch = async () => new Response(new Uint8Array(7), { headers: headers(2, 1) })
    await assert.rejects(
      rawImage('/api/image', 'a.png', 'snapshot-a', 2, 1, new AbortController().signal),
      /长度不匹配/,
    )
    await assert.rejects(
      rawImage('/api/image', 'a.png', 'snapshot-a', MAX_RAW_BYTES, 2, new AbortController().signal),
    )
  } finally {
    globalThis.fetch = old
  }
})
