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

function rawResponse(id = 'tiles') {
  return new Response(new Uint8Array(512 * 512 * 4), {
    headers: {
      'content-type': 'application/octet-stream',
      'x-pixel-format': 'BGRA8',
      'x-image-width': '512',
      'x-image-height': '512',
      'x-image-stride': '2048',
      'x-image-byte-length': String(512 * 512 * 4),
      'x-image-origin': 'top-left',
      'x-image-alpha': 'straight',
      'x-image-id': id,
    },
  })
}
test('raw tile LRU stays bounded and reset removes all retained byte arrays', async () => {
  const { TileLayer } = await import('../web/tiles.js')
  const old = globalThis.fetch,
    doc = globalThis.document
  globalThis.document = { hidden: false }
  globalThis.fetch = async () => rawResponse()
  const layer = new TileLayer(() => {})
  try {
    layer.configure({ ...info, id: 'tiles' })
    for (let i = 0; i < 80; i++)
      await layer.load(
        { key: `0/${i}/0`, x: i * 512, y: 0, width: 512, height: 512 },
        new AbortController(),
        layer.generation,
      )
    assert.equal(layer.cache.size, 64)
    assert.equal(layer.cache.values().next().value.image.format, 'BGRA8')
    layer.reset()
    assert.equal(layer.cache.size, 0)
  } finally {
    layer.dispose()
    globalThis.fetch = old
    if (doc === undefined) delete globalThis.document
    else globalThis.document = doc
  }
})
test('late raw tiles from a previous image generation are not published', async () => {
  const { TileLayer } = await import('../web/tiles.js')
  const old = globalThis.fetch,
    doc = globalThis.document
  let finish
  globalThis.document = { hidden: false }
  globalThis.fetch = () =>
    new Promise((resolve) => {
      finish = resolve
    })
  const layer = new TileLayer(() => {})
  try {
    layer.configure({ ...info, id: 'tiles' })
    const pending = layer.load({ key: '0/0/0' }, new AbortController(), layer.generation)
    layer.configure({ ...info, id: 'new-image' })
    finish(rawResponse())
    await pending
    assert.equal(layer.cache.size, 0)
  } finally {
    layer.dispose()
    globalThis.fetch = old
    if (doc === undefined) delete globalThis.document
    else globalThis.document = doc
  }
})
