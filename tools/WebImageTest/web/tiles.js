import { imageUrl } from './images.js'

export const TILE_CACHE_LIMIT = 64
export const VISIBLE_TILE_LIMIT = 48

/** Return bounded visible ROIs in original-image pixel coordinates. */
export function visibleTiles(view, info) {
  if (!info?.tiled || view.zoom <= info.preview_width / info.width) return []
  const left = Math.max(0, (0 - view.x) / view.zoom + view.imageWidth / 2)
  const top = Math.max(0, (0 - view.y) / view.zoom + view.imageHeight / 2)
  const right = Math.min(view.imageWidth, (view.width - view.x) / view.zoom + view.imageWidth / 2)
  const bottom = Math.min(
    view.imageHeight,
    (view.height - view.y) / view.zoom + view.imageHeight / 2,
  )
  if (left >= right || top >= bottom) return []
  let level = Math.min(info.max_level, Math.max(0, Math.floor(Math.log2(1 / view.zoom))))
  for (;;) {
    const step = info.tile_size * 2 ** level
    const firstX = Math.floor(left / step),
      lastX = Math.ceil(right / step) - 1
    const firstY = Math.floor(top / step),
      lastY = Math.ceil(bottom / step) - 1
    if (
      (lastX - firstX + 1) * (lastY - firstY + 1) > VISIBLE_TILE_LIMIT &&
      level < info.max_level
    ) {
      level++
      continue
    }
    const result = []
    for (let row = firstY; row <= lastY; row++) {
      for (let column = firstX; column <= lastX; column++) {
        result.push({
          key: `${level}/${column}/${row}`,
          x: column * step,
          y: row * step,
          width: Math.min(step, info.width - column * step),
          height: Math.min(step, info.height - row * step),
        })
      }
    }
    return result
  }
}

/** Four in-flight tile requests and a 64-bitmap LRU; no whole-image download. */
export class TileLayer {
  cache = new Map()
  pending = new Map()
  failed = new Set()
  wanted = []
  info = null
  generation = 0
  disposed = false
  constructor(invalidate) {
    this.invalidate = invalidate
  }
  configure(info) {
    this.reset()
    this.info = info
  }
  reset() {
    this.generation++
    for (const controller of this.pending.values()) controller.abort()
    this.pending.clear()
    for (const tile of this.cache.values()) tile.bitmap.close?.()
    this.cache.clear()
    this.failed.clear()
    this.wanted = []
    this.info = null
  }
  update(view) {
    this.wanted = visibleTiles(view, this.info)
    const wanted = new Set(this.wanted.map((tile) => tile.key))
    for (const [key, controller] of this.pending) {
      if (!wanted.has(key)) {
        controller.abort()
        this.pending.delete(key)
      }
    }
    const ready = []
    for (const tile of this.wanted) {
      const entry = this.cache.get(tile.key)
      if (!entry) continue
      this.cache.delete(tile.key)
      this.cache.set(tile.key, entry)
      ready.push(entry)
    }
    this.pump()
    return ready
  }
  pump() {
    if (this.disposed || document.hidden) return
    for (const tile of this.wanted) {
      if (this.pending.size >= 4) break
      if (this.cache.has(tile.key) || this.pending.has(tile.key) || this.failed.has(tile.key))
        continue
      const controller = new AbortController()
      const generation = this.generation
      this.pending.set(tile.key, controller)
      void this.load(tile, controller, generation)
    }
  }
  async load(tile, controller, generation) {
    let bitmap
    try {
      const response = await fetch(imageUrl(`/api/image/tile/${tile.key}`, this.info.name), {
        signal: controller.signal,
        cache: 'no-store',
      })
      if (!response.ok || !response.headers.get('content-type')?.startsWith('image/png'))
        throw new Error('Tile unavailable')
      const blob = await response.blob()
      if (blob.size > 4 * 1024 * 1024) throw new Error('Tile exceeds budget')
      bitmap = await createImageBitmap(blob, { premultiplyAlpha: 'none' })
      if (bitmap.width > this.info.tile_size || bitmap.height > this.info.tile_size)
        throw new Error('Invalid tile size')
      if (this.disposed || generation !== this.generation || controller.signal.aborted) return
      this.cache.set(tile.key, { ...tile, bitmap })
      bitmap = null
      while (this.cache.size > TILE_CACHE_LIMIT) {
        const oldest = this.cache.keys().next().value
        this.cache.get(oldest).bitmap.close?.()
        this.cache.delete(oldest)
      }
      this.invalidate()
    } catch {
      if (generation === this.generation && !controller.signal.aborted) this.failed.add(tile.key)
      // Keep the preview visible; do not spin in a retry/render loop on failures.
    } finally {
      bitmap?.close?.()
      if (this.pending.get(tile.key) === controller) this.pending.delete(tile.key)
      if (generation === this.generation && !this.disposed) this.pump()
    }
  }
  dispose() {
    this.disposed = true
    this.reset()
  }
}
