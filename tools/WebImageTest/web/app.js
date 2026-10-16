import { Viewport, MAX_IMAGE_BYTES, MAX_IMAGE_PIXELS } from './viewport.js'
import { createRenderer } from './renderer.js'
import { TileLayer } from './tiles.js'
import { imageUrl, metadata, fullImage, MAX_FULL_IMAGE_BYTES } from './images.js'

const $ = (id) => document.getElementById(id)
const host = $('viewport')
const status = $('status')
const view = new Viewport()
let renderer
let bitmap = null
let frame = 0
let controller
let sequence = 0
let lost = false
let loading = false
let disposed = false
let catalogReady = false
let listLoading = false
let listController
let requestedMode = 'tiles'
let suspended = false
const pointers = new Map()
let gesture = null
const tiles = new TileLayer(invalidate)

function setStatus(text, error = false) {
  status.textContent = text
  status.parentElement.classList.toggle('error', error)
}
function syncButtons() {
  for (const id of ['zoom-in', 'zoom-out', 'fit-image', 'actual-size'])
    $(id).disabled = !bitmap || lost
  $('load-image').disabled = loading || lost || !catalogReady
  $('image-select').disabled = !catalogReady || lost
  $('refresh-images').disabled = listLoading
  $('display-mode').disabled = lost
}
function draw() {
  frame = 0
  if (disposed || lost || document.hidden || !renderer) return
  try {
    const ready = suspended ? [] : tiles.update(view)
    renderer.setTiles(ready)
    host.dataset.tiles = String(ready.length)
    renderer.draw(view)
  } catch {
    useCanvasFallback()
    renderer.setTiles(suspended ? [] : tiles.update(view))
    renderer.draw(view)
  }
  $('zoom-value').textContent = bitmap ? `${(view.zoom * 100).toFixed(1)}%` : '—'
}
function invalidate() {
  if (!frame && !disposed && !lost && !document.hidden) frame = requestAnimationFrame(draw)
}
function attachContextEvents() {
  const canvas = renderer.canvas
  canvas.addEventListener('webglcontextlost', (event) => {
    event.preventDefault()
    if (renderer.canvas !== canvas || disposed) return
    lost = true
    sequence++
    controller?.abort()
    loading = false
    if (frame) cancelAnimationFrame(frame)
    frame = 0
    setStatus('GPU 上下文已丢失，等待浏览器恢复；内存图片仍保留。', true)
    syncButtons()
  })
  canvas.addEventListener('webglcontextrestored', () => {
    if (renderer.canvas !== canvas || disposed) return
    try {
      // Context restoration invalidates all old GPU objects; re-create from the retained bitmap.
      renderer = createRenderer(canvas)
      if (bitmap) renderer.setImage(bitmap)
      lost = false
      $('renderer-name').textContent = renderer.name
      setStatus('绘制器已恢复。')
      syncButtons()
      invalidate()
    } catch {
      useCanvasFallback()
      lost = false
      syncButtons()
      invalidate()
    }
  })
}
function useCanvasFallback() {
  renderer.dispose()
  renderer = createRenderer(renderer.canvas, true)
  if (bitmap) renderer.setImage(bitmap)
  $('renderer-name').textContent = renderer.name
  setStatus('已切换 Canvas2D，缩放与平移仍可使用。')
}

async function decode(blob) {
  if (typeof createImageBitmap === 'function')
    return createImageBitmap(blob, { premultiplyAlpha: 'none' })
  const url = URL.createObjectURL(blob)
  try {
    const image = new Image()
    image.src = url
    await image.decode()
    return image
  } finally {
    URL.revokeObjectURL(url)
  }
}
function stopSelection() {
  sequence++
  controller?.abort()
  loading = false
  suspended = true
  renderer?.setTiles([])
  tiles.reset()
  pointers.clear()
  gesture = null
  host.classList.remove('dragging')
  syncButtons()
  invalidate()
}

async function refreshImages() {
  listController?.abort()
  const current = new AbortController()
  listController = current
  listLoading = true
  syncButtons()
  try {
    const response = await fetch('/api/images', { signal: current.signal, cache: 'no-store' })
    if (!response.ok) throw new Error(`图片列表查询失败：HTTP ${response.status}`)
    const data = await response.json()
    if (!Array.isArray(data.items)) throw new Error('图片列表格式无效。')
    if (disposed || current !== listController) return
    const previous = $('image-select').value
    const options = data.items.map((item) => {
      if (typeof item.name !== 'string' || !Number.isFinite(item.size_bytes))
        throw new Error('图片列表格式无效。')
      const option = new Option(
        `${item.name} · ${(item.size_bytes / 1048576).toFixed(1)} MiB`,
        item.name,
      )
      option.disabled = item.loadable === false
      return option
    })
    $('image-select').replaceChildren(...options)
    const usable = options.filter((option) => !option.disabled)
    const selected =
      usable.find((option) => option.value === previous) ||
      usable.find((option) => option.value === data.default) ||
      usable[0]
    catalogReady = !!selected
    if (selected) $('image-select').value = selected.value
    else $('image-select').replaceChildren(new Option('目录中没有可加载的图片', ''))
    if (previous && previous !== selected?.value) stopSelection()
    if (!loading)
      setStatus(
        selected
          ? `发现 ${options.length} 张候选图片，选择后点击获取。`
          : '目录中没有可加载的图片，请添加图片后刷新列表。',
      )
  } catch (error) {
    if (!disposed && current === listController && error.name !== 'AbortError')
      setStatus(error.message, true)
  } finally {
    if (current === listController) {
      listLoading = false
      syncButtons()
    }
  }
}

async function loadImage() {
  stopSelection()
  const current = ++sequence
  const name = $('image-select').value
  const mode = $('display-mode').value
  controller = new AbortController()
  loading = true
  syncButtons()
  setStatus('正在请求并解码内存图片…')
  let next = null
  try {
    const info = await metadata(name, controller.signal)
    if (current !== sequence || disposed) return
    const tiled = mode === 'tiles' && info.tiled
    if (
      !Number.isSafeInteger(info.width) ||
      !Number.isSafeInteger(info.height) ||
      info.width < 1 ||
      info.height < 1 ||
      info.width * info.height > MAX_IMAGE_PIXELS
    )
      throw new Error('原图尺寸无效或超出像素上限。')
    if (
      tiled &&
      (!Number.isInteger(info.tile_size) ||
        info.tile_size < 1 ||
        info.tile_size > 512 ||
        !Number.isInteger(info.max_level) ||
        info.max_level < 0 ||
        info.max_level > 20 ||
        !Number.isInteger(info.preview_width) ||
        info.preview_width < 1 ||
        info.preview_width > 2048)
    )
      throw new Error('瓦片配置无效。')
    const endpoint = tiled ? '/api/image/preview' : '/api/image'
    const maxBytes = mode === 'full' ? MAX_FULL_IMAGE_BYTES : MAX_IMAGE_BYTES
    let blob
    if (mode === 'full' && info.encoded_bytes > 16 * 1024 * 1024) {
      blob = await fullImage(name, info.encoded_bytes, controller.signal, (progress) => {
        if (current === sequence)
          setStatus(`正在下载完整原图：${(progress * 100).toFixed(0)}%（非瓦片）`)
      })
    } else {
      const response = await fetch(imageUrl(endpoint, name), {
        signal: controller.signal,
        cache: 'no-store',
      })
      if (!response.ok) throw new Error(`获取失败：HTTP ${response.status}`)
      if (response.headers.get('content-type')?.split(';')[0] !== 'image/png')
        throw new Error('服务未返回 PNG 图片。')
      const length = Number(response.headers.get('content-length'))
      if (length > maxBytes) throw new Error('图片响应超过当前模式的大小上限。')
      blob = await response.blob()
    }
    if (!blob.size || blob.size > maxBytes) throw new Error('图片为空或超出大小限制。')
    next = await decode(blob)
    if (current !== sequence || disposed) return
    const width = next.width || next.naturalWidth
    const height = next.height || next.naturalHeight
    if (
      width < 1 ||
      height < 1 ||
      width * height > (mode === 'full' ? MAX_IMAGE_PIXELS : 64 * 1024 * 1024)
    )
      throw new Error('解码后的图片超出模式像素上限或尺寸无效。')
    try {
      if (requestedMode !== mode && renderer.name.startsWith('Canvas2D')) {
        renderer.dispose()
        renderer = createRenderer(renderer.canvas)
        attachContextEvents()
      }
      renderer.setImage(next)
    } catch {
      useCanvasFallback()
      renderer.setImage(next)
    }
    const previous = bitmap
    bitmap = next
    next = null
    previous?.close?.()
    view.setImage(info.width, info.height)
    tiles.configure(tiled ? { ...info, name } : null)
    suspended = false
    requestedMode = mode
    $('empty-state').hidden = true
    $('image-info').textContent =
      `${name} · ${info.width} × ${info.height} px · ${tiled ? '瓦片模式 · 预览' : '整图'} ${(blob.size / 1024).toFixed(1)} KiB`
    $('renderer-name').textContent = renderer.name
    host.dataset.loaded = 'true'
    host.dataset.name = name
    host.dataset.mode = mode
    setStatus(
      mode === 'full' && renderer.name.startsWith('Canvas2D')
        ? '整图已载入 Canvas2D：原图超过单纹理上限或 WebGL 不可用，未使用瓦片。'
        : '图片已就绪。滚轮缩放，拖动平移，双击适应窗口。',
    )
    invalidate()
  } catch (error) {
    if (current === sequence && !disposed && error.name !== 'AbortError') {
      setStatus(error.message || '图片请求失败。', true)
      suspended = true
    }
  } finally {
    next?.close?.()
    if (current === sequence) {
      loading = false
      syncButtons()
    }
  }
}

function localPoint(event) {
  const rect = host.getBoundingClientRect()
  return { x: event.clientX - rect.left, y: event.clientY - rect.top }
}
function pinch() {
  const [a, b] = [...pointers.values()]
  return { x: (a.x + b.x) / 2, y: (a.y + b.y) / 2, distance: Math.hypot(b.x - a.x, b.y - a.y) }
}
function release(event) {
  if (!pointers.has(event.pointerId)) return
  pointers.delete(event.pointerId)
  gesture = pointers.size >= 2 ? pinch() : null
  host.classList.toggle('dragging', pointers.size > 0)
  if (host.hasPointerCapture(event.pointerId)) host.releasePointerCapture(event.pointerId)
}
host.addEventListener('pointerdown', (event) => {
  if (!bitmap || lost || event.button !== 0 || pointers.size >= 2) return
  host.focus({ preventScroll: true })
  pointers.set(event.pointerId, localPoint(event))
  host.setPointerCapture(event.pointerId)
  host.classList.add('dragging')
  if (pointers.size === 2) gesture = pinch()
})
host.addEventListener('pointermove', (event) => {
  if (!pointers.has(event.pointerId)) return
  const previous = pointers.get(event.pointerId)
  const point = localPoint(event)
  pointers.set(event.pointerId, point)
  if (pointers.size === 2) {
    const next = pinch()
    if (gesture) {
      view.pan(next.x - gesture.x, next.y - gesture.y)
      if (gesture.distance > 0 && next.distance > 0)
        view.zoomAt(next.x, next.y, (view.zoom * next.distance) / gesture.distance)
    }
    gesture = next
  } else view.pan(point.x - previous.x, point.y - previous.y)
  invalidate()
})
for (const name of ['pointerup', 'pointercancel', 'lostpointercapture'])
  host.addEventListener(name, release)
host.addEventListener(
  'wheel',
  (event) => {
    if (!bitmap || lost) return
    event.preventDefault()
    const point = localPoint(event)
    const delta =
      event.deltaY * (event.deltaMode === 1 ? 16 : event.deltaMode === 2 ? view.height : 1)
    view.zoomAt(
      point.x,
      point.y,
      view.zoom * Math.exp(-Math.max(-1000, Math.min(1000, delta)) * 0.0015),
    )
    invalidate()
  },
  { passive: false },
)
host.addEventListener('dblclick', () => {
  view.fit()
  invalidate()
})
host.addEventListener('keydown', (event) => {
  if (!bitmap || lost) return
  const actions = {
    '+': () => view.zoomAt(view.width / 2, view.height / 2, view.zoom * 1.25),
    '=': () => view.zoomAt(view.width / 2, view.height / 2, view.zoom * 1.25),
    '-': () => view.zoomAt(view.width / 2, view.height / 2, view.zoom / 1.25),
    0: () => view.fit(),
    1: () => view.actualSize(),
    ArrowLeft: () => view.pan(30, 0),
    ArrowRight: () => view.pan(-30, 0),
    ArrowUp: () => view.pan(0, 30),
    ArrowDown: () => view.pan(0, -30),
  }
  if (actions[event.key]) {
    event.preventDefault()
    actions[event.key]()
    invalidate()
  }
})
$('load-image').addEventListener('click', loadImage)
$('refresh-images').addEventListener('click', refreshImages)
for (const id of ['image-select', 'display-mode'])
  $(id).addEventListener('change', () => {
    stopSelection()
    setStatus(
      $('display-mode').value === 'full'
        ? '整图模式将下载并解码完整原图，大图内存开销较高；点击获取开始加载。'
        : '已选择图片和瓦片模式，点击获取开始加载。',
    )
  })
$('zoom-in').addEventListener('click', () => {
  view.zoomAt(view.width / 2, view.height / 2, view.zoom * 1.25)
  invalidate()
})
$('zoom-out').addEventListener('click', () => {
  view.zoomAt(view.width / 2, view.height / 2, view.zoom / 1.25)
  invalidate()
})
$('fit-image').addEventListener('click', () => {
  view.fit()
  invalidate()
})
$('actual-size').addEventListener('click', () => {
  view.actualSize()
  invalidate()
})
$('fullscreen').disabled = !document.fullscreenEnabled
$('fullscreen').addEventListener('click', async () => {
  try {
    if (document.fullscreenElement) await document.exitFullscreen()
    else await document.querySelector('.viewer-panel').requestFullscreen()
  } catch {
    setStatus('当前浏览器不允许全屏。', true)
  }
})
const resize = new ResizeObserver(() => {
  view.resize(host.clientWidth, host.clientHeight)
  invalidate()
})
function visibility() {
  if (document.hidden) {
    if (frame) cancelAnimationFrame(frame)
    frame = 0
  } else invalidate()
}
function windowResize() {
  invalidate()
}
function dispose() {
  disposed = true
  sequence++
  controller?.abort()
  listController?.abort()
  if (frame) cancelAnimationFrame(frame)
  frame = 0
  resize.disconnect()
  document.removeEventListener('visibilitychange', visibility)
  window.removeEventListener('resize', windowResize)
  renderer?.dispose()
  tiles.dispose()
  bitmap?.close?.()
  bitmap = null
}
window.addEventListener('pagehide', (event) => {
  if (!event.persisted) dispose()
})
document.addEventListener('visibilitychange', visibility)
window.addEventListener('resize', windowResize, { passive: true })
try {
  renderer = createRenderer($('image-canvas'))
  attachContextEvents()
  $('renderer-name').textContent = renderer.name
  resize.observe(host)
  syncButtons()
  invalidate()
  void refreshImages()
} catch {
  setStatus('无法创建绘制器，请更新浏览器或启用硬件加速。', true)
  $('load-image').disabled = true
}
