import test from 'node:test'
import assert from 'node:assert/strict'
import { imageUrl, metadata, fullImage, MAX_FULL_IMAGE_BYTES } from '../web/images.js'

test('image identity is URL encoded consistently for metadata, PNG and tiles', () => {
  for (const path of ['/api/image', '/api/image/info', '/api/image/tile/0/1/2']) {
    const url = new URL(imageUrl(path, '蓝色 图片 #2.PPM'), 'http://localhost')
    assert.equal(url.searchParams.get('name'), '蓝色 图片 #2.PPM')
    assert.equal(url.pathname, path)
    assert.equal(url.hash, '')
  }
  assert.equal(imageUrl('/api/image'), '/api/image')
})

test('metadata does not turn a terminal decode failure into a retry loop', async () => {
  const previous = globalThis.fetch
  let calls = 0
  globalThis.fetch = async () => {
    calls++
    return new Response(JSON.stringify({ code: 1003 }), {
      status: 422,
      headers: { 'content-type': 'application/json' },
    })
  }
  try {
    await assert.rejects(metadata('bad.png', new AbortController().signal), /解码失败/)
    assert.equal(calls, 1)
  } finally {
    globalThis.fetch = previous
  }
})

test('changing selection aborts metadata polling promptly', async () => {
  const previous = globalThis.fetch
  const controller = new AbortController()
  globalThis.fetch = async () =>
    new Response(JSON.stringify({ code: 1013 }), {
      status: 202,
      headers: { 'content-type': 'application/json' },
    })
  try {
    const pending = metadata('big.png', controller.signal)
    setImmediate(() => controller.abort())
    await assert.rejects(pending, (error) => error.name === 'AbortError')
  } finally {
    globalThis.fetch = previous
  }
})

test('non-tiled full download assembles exact PNG bytes from bounded ranges', async () => {
  const previous = globalThis.fetch
  const bytes = new Uint8Array(1024 * 1024 + 17)
  bytes.fill(37)
  bytes[bytes.length - 1] = 93
  const ranges = []
  globalThis.fetch = async (url, options) => {
    const match = options.headers.Range.match(/^bytes=(\d+)-(\d+)$/)
    const start = Number(match[1]),
      end = Number(match[2])
    assert.equal(new URL(url, 'http://localhost').searchParams.get('name'), 'test.png')
    assert.ok(end - start < 1024 * 1024)
    ranges.push([start, end])
    return new Response(bytes.slice(start, end + 1), {
      status: 206,
      headers: {
        'content-type': 'image/png',
        'content-range': `bytes ${start}-${end}/${bytes.length}`,
      },
    })
  }
  try {
    const result = await fullImage('test.png', bytes.length, new AbortController().signal)
    assert.equal(result.type, 'image/png')
    assert.deepEqual(new Uint8Array(await result.arrayBuffer()), bytes)
    assert.deepEqual(ranges, [
      [0, 1048575],
      [1048576, 1048592],
    ])
  } finally {
    globalThis.fetch = previous
  }
})

test('full image transfer rejects mismatched ranges and unsupported budgets', async () => {
  const previous = globalThis.fetch
  globalThis.fetch = async () =>
    new Response('bad', {
      status: 206,
      headers: { 'content-type': 'image/png', 'content-range': 'bytes 1-3/3' },
    })
  try {
    await assert.rejects(fullImage('x.png', 3, new AbortController().signal), /分段下载失败/)
    await assert.rejects(
      fullImage('x.png', MAX_FULL_IMAGE_BYTES + 1, new AbortController().signal),
      /大小无效/,
    )
  } finally {
    globalThis.fetch = previous
  }
})
