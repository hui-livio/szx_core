import test from 'node:test'
import assert from 'node:assert/strict'
import { liveFrame } from '../web/images.js'
import { LiveStream } from '../web/live.js'

const width = 2448,
  height = 2048,
  length = width * height * 4
const headers = {
  'content-type': 'application/octet-stream',
  'x-pixel-format': 'BGRA8',
  'x-image-id': 'live-1792641600123',
  'x-image-width': String(width),
  'x-image-height': String(height),
  'x-image-stride': String(width * 4),
  'x-image-byte-length': String(length),
  'x-image-origin': 'top-left',
  'x-image-alpha': 'straight',
  'x-frame-timestamp-ms': '1792641600123',
  'x-frame-time': '2026-10-22 04:00:00.123 UTC',
}

test('live frame receives exactly 2448×2048 packed BGRA8 without Range or encoding', async (t) => {
  const bytes = new Uint8Array(length)
  bytes.set([17, 83, 211, 255])
  bytes[length - 1] = 255
  const controller = new AbortController()
  t.mock.method(globalThis, 'fetch', async (url, options) => {
    assert.equal(url, '/api/live/frame')
    assert.equal(options.signal, controller.signal)
    assert.equal(options.cache, 'no-store')
    assert.equal(options.headers, undefined)
    return new Response(bytes, { headers })
  })
  const frame = await liveFrame(controller.signal)
  assert.equal(frame.width, width)
  assert.equal(frame.height, height)
  assert.equal(frame.data.length, 20054016)
  assert.deepEqual([...frame.data.subarray(0, 4)], [17, 83, 211, 255])
  assert.equal(frame.data.at(-1), 255)
  assert.equal(frame.format, 'BGRA8')
  assert.equal(frame.time, headers['x-frame-time'])
  assert.equal(frame.timestamp, 1792641600123)
})

test('live frame rejects wrong size, layout, time, encoding, truncation and server errors', async (t) => {
  for (const patch of [
    { 'x-image-width': '1920' },
    { 'x-image-stride': '1' },
    { 'x-pixel-format': 'RGBA8' },
    { 'x-image-id': 'snapshot-old' },
    { 'x-frame-timestamp-ms': 'invalid' },
    { 'x-frame-time': '' },
    { 'content-type': 'image/png' },
    {},
  ]) {
    t.mock.method(
      globalThis,
      'fetch',
      async () => new Response(new Uint8Array(4), { headers: { ...headers, ...patch } }),
    )
    await assert.rejects(liveFrame(new AbortController().signal))
  }
  t.mock.method(globalThis, 'fetch', async () => new Response('{"code":1007}', { status: 503 }))
  await assert.rejects(liveFrame(new AbortController().signal), /内存不足/)
})

function clock(t) {
  let now = 0
  t.mock.method(performance, 'now', () => now)
  t.mock.timers.enable({ apis: ['setTimeout'] })
  return async (ms) => {
    now += ms
    t.mock.timers.tick(ms)
    await new Promise(setImmediate)
  }
}

test('target FPS controls request starts and changes take effect while running', async (t) => {
  const advance = clock(t)
  let requests = 0,
    frames = 0
  const stream = new LiveStream(
    () => frames++,
    assert.fail,
    async () => ++requests,
  )
  t.after(() => stream.stop())
  stream.setFps(1)
  stream.start()
  await advance(0)
  assert.equal(frames, 1)
  await advance(500)
  assert.equal(requests, 1)
  stream.setFps(60)
  await advance(0)
  assert.equal(frames, 2)
  await advance(16)
  assert.equal(requests, 2)
  await advance(1)
  assert.equal(frames, 3)
  stream.setFps(1)
  await advance(999)
  assert.equal(requests, 3)
  await advance(1)
  assert.equal(requests, 4)
  for (const invalid of [0, 61, 1.5, NaN, Infinity]) assert.throws(() => stream.setFps(invalid))
})

test('slow transfer never overlaps requests and stop cancels fetch and pending timers', async (t) => {
  const advance = clock(t)
  const pending = []
  const frames = []
  const stream = new LiveStream(
    (frame) => frames.push(frame),
    assert.fail,
    (signal) => new Promise((resolve) => pending.push({ signal, resolve })),
  )
  t.after(() => stream.stop())
  stream.setFps(60)
  stream.start()
  await advance(0)
  await advance(1000)
  assert.equal(pending.length, 1)
  pending[0].resolve('first')
  await advance(0)
  await advance(0)
  assert.equal(pending.length, 2)
  stream.stop()
  assert.equal(pending[1].signal.aborted, true)
  pending[1].resolve('stale')
  await advance(2000)
  assert.deepEqual(frames, ['first'])
  assert.equal(pending.length, 2)
})

test('restart ignores late results from the previous generation', async (t) => {
  const advance = clock(t)
  const pending = [],
    frames = []
  const stream = new LiveStream(
    (frame) => frames.push(frame),
    assert.fail,
    () => new Promise((resolve) => pending.push(resolve)),
  )
  t.after(() => stream.stop())
  stream.start()
  await advance(0)
  stream.start()
  await advance(0)
  pending[0]('old')
  pending[1]('new')
  await advance(0)
  assert.deepEqual(frames, ['new'])
  stream.stop()
  await advance(2000)
  assert.equal(pending.length, 2)
})

test('request or renderer failures stop the stream and are reported once', async (t) => {
  const advance = clock(t)
  for (const renderFailure of [false, true]) {
    const errors = []
    const failure = new Error(renderFailure ? 'renderer failed' : 'request failed')
    const stream = new LiveStream(
      () => {
        throw failure
      },
      (error) => errors.push(error),
      async () => {
        if (!renderFailure) throw failure
        return {}
      },
    )
    stream.start()
    await advance(0)
    await advance(2000)
    assert.equal(stream.running, false)
    assert.deepEqual(errors, [failure])
  }
})
