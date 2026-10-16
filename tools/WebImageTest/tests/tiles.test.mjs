import test from 'node:test'
import assert from 'node:assert/strict'
import { Viewport } from '../web/viewport.js'
import { visibleTiles, TILE_CACHE_LIMIT, VISIBLE_TILE_LIMIT } from '../web/tiles.js'

const info = {
  width: 17952,
  height: 13200,
  tiled: true,
  preview_width: 2048,
  tile_size: 512,
  max_level: 6,
}
test('moban dimensions fit view and use preview without full-image tiles', () => {
  const view = new Viewport()
  view.resize(1200, 700)
  view.setImage(info.width, info.height)
  assert.deepEqual(visibleTiles(view, info), [])
  view.actualSize()
  const tiles = visibleTiles(view, info)
  assert.ok(tiles.length > 0 && tiles.length <= VISIBLE_TILE_LIMIT)
  for (const tile of tiles) {
    assert.match(tile.key, /^0\//)
    assert.ok(tile.width <= 512 && tile.height <= 512)
  }
})
test('visible tiles never exceed budgets and stay within original-image bounds', () => {
  for (const [width, height] of [
    [320, 480],
    [1920, 1080],
    [3840, 2160],
    [16000, 12000],
  ]) {
    const view = new Viewport()
    view.resize(width, height)
    view.setImage(info.width, info.height)
    for (const zoom of [0.01, 0.15, 0.25, 0.5, 1, 5, 64]) {
      view.zoomAt(width / 2, height / 2, zoom)
      for (const offset of [-20000, 0, 20000]) {
        view.x = width / 2 + offset
        const tiles = visibleTiles(view, info)
        assert.ok(tiles.length <= VISIBLE_TILE_LIMIT)
        assert.ok(tiles.length < TILE_CACHE_LIMIT)
        for (const tile of tiles) {
          assert.ok(tile.x >= 0 && tile.y >= 0)
          assert.ok(tile.x + tile.width <= info.width)
          assert.ok(tile.y + tile.height <= info.height)
          assert.ok(tile.width > 0 && tile.height > 0)
        }
      }
    }
  }
})
test('fully offscreen image needs no tiles and small images keep the single-texture path', () => {
  const view = new Viewport()
  view.resize(1000, 600)
  view.setImage(info.width, info.height)
  view.actualSize()
  view.x = 1e9
  assert.deepEqual(visibleTiles(view, info), [])
  assert.deepEqual(visibleTiles(view, { ...info, tiled: false }), [])
})

test('tile bitmap LRU evicts and closes resources; reset closes all remaining entries', async () => {
  const { TileLayer } = await import('../web/tiles.js')
  const oldFetch = globalThis.fetch,
    oldBitmap = globalThis.createImageBitmap,
    oldDocument = globalThis.document
  let closed = 0
  globalThis.document = { hidden: false }
  globalThis.fetch = async () => ({
    ok: true,
    headers: new Headers({ 'content-type': 'image/png' }),
    blob: async () => new Blob(['tile']),
  })
  globalThis.createImageBitmap = async () => ({
    width: 512,
    height: 512,
    close: () => {
      closed++
    },
  })
  const layer = new TileLayer(() => {})
  try {
    layer.configure(info)
    for (let i = 0; i < 80; i++) {
      await layer.load(
        { key: `0/${i}/0`, x: i * 512, y: 0, width: 512, height: 512 },
        new AbortController(),
        layer.generation,
      )
    }
    assert.equal(layer.cache.size, 64)
    assert.equal(closed, 16)
    layer.reset()
    assert.equal(closed, 80)
    assert.equal(layer.cache.size, 0)
  } finally {
    layer.dispose()
    globalThis.fetch = oldFetch
    globalThis.createImageBitmap = oldBitmap
    if (oldDocument === undefined) delete globalThis.document
    else globalThis.document = oldDocument
  }
})

test('image generation changes close late decoded tiles instead of publishing stale content', async () => {
  const { TileLayer } = await import('../web/tiles.js')
  const oldFetch = globalThis.fetch,
    oldBitmap = globalThis.createImageBitmap,
    oldDocument = globalThis.document
  let finish,
    closed = 0
  globalThis.document = { hidden: false }
  globalThis.fetch = async () => ({
    ok: true,
    headers: new Headers({ 'content-type': 'image/png' }),
    blob: async () => new Blob(['tile']),
  })
  globalThis.createImageBitmap = () =>
    new Promise((resolve) => {
      finish = resolve
    })
  const layer = new TileLayer(() => {})
  try {
    layer.configure(info)
    const pending = layer.load({ key: '0/0/0' }, new AbortController(), layer.generation)
    while (!finish) await new Promise((resolve) => setImmediate(resolve))
    layer.configure(info)
    finish({
      width: 512,
      height: 512,
      close: () => {
        closed++
      },
    })
    await pending
    assert.equal(layer.cache.size, 0)
    assert.equal(closed, 1)
  } finally {
    layer.dispose()
    globalThis.fetch = oldFetch
    globalThis.createImageBitmap = oldBitmap
    if (oldDocument === undefined) delete globalThis.document
    else globalThis.document = oldDocument
  }
})
