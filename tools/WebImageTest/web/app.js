import { Viewport } from './viewport.js'
import { pixelRoi } from './roi.js'
import { createRenderer } from './renderer.js'
import { TileLayer } from './tiles.js'
import { loadBackend, imageData } from './images.js'

const $ = (id) => document.getElementById(id)
const host = $('viewport')
const status = $('status')
const view = new Viewport()
let renderer
let image = null
let frame = 0
let controller
let sequence = 0
let lost = false
let loading = false
let backendLoading = false
let backendController
let backendSequence = 0
let readyInfo = null
let disposed = false
let catalogReady = false
let listLoading = false
let listController
let requestedMode = 'tiles'
let encoding = 'raw'
let suspended = false
const pointers = new Map()
let gesture = null
let roiMode = false
let roi = null
let roiDrag = null
const tiles = new TileLayer(invalidate)

function setStatus(text, error = false) {
  status.textContent = text
  status.parentElement.classList.toggle('error', error)
}
function syncButtons() {
  for (const id of ['zoom-in', 'zoom-out', 'fit-image', 'actual-size'])
    $(id).disabled = !image || lost
  $('draw-roi').disabled = !image || lost || loading || backendLoading
  $('clear-roi').disabled = !roi && !roiDrag
  $('load-backend').disabled = backendLoading || loading || !catalogReady || lost
  $('load-image').disabled = loading || backendLoading || lost || !catalogReady || !readyInfo
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
  drawRoi()
  $('zoom-value').textContent = image ? `${(view.zoom * 100).toFixed(1)}%` : '—'
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
    cancelInteraction()
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
      // Context restoration invalidates all old GPU objects; re-create from the retained image.
      renderer = createRenderer(canvas)
      if (image) renderer.setImage(image)
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
  if (image) renderer.setImage(image)
  $('renderer-name').textContent = renderer.name
  setStatus('已切换 Canvas2D，缩放与平移仍可使用。')
}

function drawRoi() {
  const bounds = roiDrag ? roiDrag.bounds : roi
  const outline = $('roi-outline')
  outline.hidden = !bounds
  if (bounds) {
    const point = view.screenPoint(bounds)
    outline.style.left = `${point.x}px`
    outline.style.top = `${point.y}px`
    outline.style.width = `${bounds.w * view.zoom}px`
    outline.style.height = `${bounds.h * view.zoom}px`
  }
  $('roi-info').hidden = !roi || !!roiDrag
  if (roi) for (const key of ['x', 'y', 'w', 'h']) $(`roi-${key}`).textContent = roi[key]
}
function cancelInteraction() {
  const captured = [...pointers.keys()]
  pointers.clear()
  gesture = null
  roiDrag = null
  for (const id of captured) if (host.hasPointerCapture(id)) host.releasePointerCapture(id)
  host.classList.remove('dragging')
  syncButtons()
  invalidate()
}
function clearRoi() {
  cancelInteraction()
  roi = null
  syncButtons()
  invalidate()
}
function setRoiMode(enabled) {
  cancelInteraction()
  roiMode = enabled
  host.classList.toggle('roi-mode', enabled)
  $('draw-roi').setAttribute('aria-pressed', String(enabled))
}

function stopSelection() {
  clearRoi()
  backendSequence++
  backendController?.abort()
  backendLoading = false
  readyInfo = null
  host.dataset.backendState = 'not-loaded'
  stopTransfer()
}
function stopTransfer() {
  $('transfer-time').textContent = '整图获取耗时：—'
  delete $('transfer-time').dataset.ms
  sequence++
  controller?.abort()
  loading = false
  suspended = true
  renderer?.setTiles([])
  tiles.reset()
  cancelInteraction()
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

async function prepareBackend() {
  stopSelection()
  const current = ++backendSequence
  const name = $('image-select').value
  backendController = new AbortController()
  backendLoading = true
  host.dataset.backendState = 'loading'
  syncButtons()
  setStatus('后端正在读取、解码并转换为连续 BGRA8…')
  try {
    const info = await loadBackend(name, backendController.signal)
    if (disposed || current !== backendSequence) return
    readyInfo = { ...info, name }
    host.dataset.backendState = 'ready'
    host.dataset.backendId = info.id
    setStatus(
      `后端 BGRA8 已加载：${info.width} × ${info.height}，${(info.byte_length / 1048576).toFixed(1)} MiB。点击获取内存图片。`,
    )
  } catch (error) {
    if (!disposed && current === backendSequence && error.name !== 'AbortError') {
      host.dataset.backendState = 'failed'
      setStatus(error.message || '后端 BGRA8 加载失败。', true)
    }
  } finally {
    if (current === backendSequence) {
      backendLoading = false
      syncButtons()
    }
  }
}

async function loadImage() {
  if (!readyInfo || readyInfo.name !== $('image-select').value) return
  stopTransfer()
  const current = ++sequence
  const info = readyInfo
  const name = info.name
  const mode = $('display-mode').value
  const requestedEncoding = encoding
  const tiled = mode === 'tiles' && info.tiled
  const transferLabel = tiled ? '预览图获取耗时' : '整图获取耗时'
  let received = false
  $('transfer-time').textContent = `${transferLabel}：接收中…`
  controller = new AbortController()
  loading = true
  syncButtons()
  setStatus(
    requestedEncoding === 'raw'
      ? '正在获取已解码的 BGRA8 原始字节…'
      : `正在从 BGRA8 内存原图获取 ${requestedEncoding.toUpperCase()}…`,
  )
  try {
    const next = await imageData(
      tiled ? '/api/image/preview' : '/api/image',
      name,
      info.id,
      tiled ? info.preview_width : info.width,
      tiled ? info.preview_height : info.height,
      controller.signal,
      (progress) => {
        if (current === sequence)
          setStatus(`正在传输 BGRA8 原始字节：${(progress * 100).toFixed(0)}%`)
      },
      requestedEncoding,
      (transfer) => {
        if (disposed || current !== sequence) return
        received = true
        $('transfer-time').dataset.ms = String(transfer.transferMs)
        $('transfer-time').textContent =
          `${transferLabel}：${transfer.transferMs.toFixed(1)} ms（${(transfer.transferMs / 1000).toFixed(3)} s）`
        if (requestedEncoding !== 'raw') setStatus('数据已接收完成，正在解码图片…')
      },
    )
    if (disposed || current !== sequence) return
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
    clearRoi()
    image = next
    view.setImage(info.width, info.height)
    tiles.configure(tiled ? { ...info, encoding: requestedEncoding } : null)
    suspended = false
    requestedMode = mode
    $('empty-state').hidden = true
    $('image-info').textContent =
      `${name} · ${info.width} × ${info.height} px · 内存 BGRA8 · ${requestedEncoding === 'raw' ? '不编码' : requestedEncoding.toUpperCase()} · ${tiled ? '瓦片预览' : '整图'} ${(next.transferBytes / 1048576).toFixed(2)} MiB`
    $('renderer-name').textContent = renderer.name
    host.dataset.loaded = 'true'
    host.dataset.name = name
    host.dataset.mode = mode
    host.dataset.format = 'BGRA8'
    host.dataset.encoding = requestedEncoding
    host.dataset.snapshot = info.id
    setStatus(
      requestedEncoding !== 'raw'
        ? `${requestedEncoding.toUpperCase()} 已接收并解码显示；服务端内存原图仍为 BGRA8，接收耗时不包含浏览器解码。`
        : mode === 'full' && renderer.name.startsWith('Canvas2D')
          ? 'BGRA8 整图已载入，使用 Canvas2D 可见区域 CPU 转换降级；没有压缩图解码或瓦片请求。'
          : 'BGRA8 已显示；原始字节直接上传纹理，没有浏览器图片解码。',
    )
    invalidate()
  } catch (error) {
    if (!disposed && current === sequence && error.name !== 'AbortError') {
      if (!received) $('transfer-time').textContent = `${transferLabel}：接收失败`
      setStatus(error.message || 'BGRA8 数据请求失败。', true)
      suspended = true
    }
  } finally {
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
  if (roiDrag?.pointerId === event.pointerId) {
    if (event.type === 'pointerup') {
      roi = pixelRoi(
        roiDrag.start,
        view.imagePoint(localPoint(event)),
        view.imageWidth,
        view.imageHeight,
      )
    }
    roiDrag = null
    syncButtons()
    invalidate()
  }
  pointers.delete(event.pointerId)
  gesture = pointers.size >= 2 ? pinch() : null
  host.classList.toggle('dragging', pointers.size > 0)
  if (host.hasPointerCapture(event.pointerId)) host.releasePointerCapture(event.pointerId)
}
host.addEventListener('pointerdown', (event) => {
  if (!image || lost || event.button !== 0 || pointers.size >= 2) return
  if (roiMode && pointers.size) return
  const point = view.imagePoint(localPoint(event))
  if (roiMode) {
    if (point.x < 0 || point.y < 0 || point.x >= view.imageWidth || point.y >= view.imageHeight)
      return
    roiDrag = { pointerId: event.pointerId, start: point, bounds: null }
    syncButtons()
    invalidate()
  }
  host.focus({ preventScroll: true })
  pointers.set(event.pointerId, localPoint(event))
  host.setPointerCapture(event.pointerId)
  if (!roiMode) host.classList.add('dragging')
  if (pointers.size === 2) gesture = pinch()
})
host.addEventListener('pointermove', (event) => {
  if (!pointers.has(event.pointerId)) return
  if (roiDrag?.pointerId === event.pointerId) {
    roiDrag.bounds = pixelRoi(
      roiDrag.start,
      view.imagePoint(localPoint(event)),
      view.imageWidth,
      view.imageHeight,
    )
    invalidate()
    return
  }
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
    if (!image || lost) return
    event.preventDefault()
    if (roiDrag) return
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
  if (roiMode) return
  view.fit()
  invalidate()
})
host.addEventListener('keydown', (event) => {
  if (!image || lost) return
  if (event.key === 'Escape') {
    event.preventDefault()
    setRoiMode(false)
    return
  }
  if (roiDrag) return
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
$('draw-roi').addEventListener('click', () => {
  setRoiMode(!roiMode)
  host.focus({ preventScroll: true })
})
$('clear-roi').addEventListener('click', clearRoi)
$('load-backend').addEventListener('click', prepareBackend)
$('load-image').addEventListener('click', loadImage)
$('refresh-images').addEventListener('click', refreshImages)
$('image-select').addEventListener('change', () => {
  stopSelection()
  setStatus('已选择图片，请先点击后端加载并解码为 BGRA8。')
})
for (const checkbox of document.querySelectorAll('#encoding-options input')) {
  checkbox.addEventListener('change', () => {
    const changed = encoding !== checkbox.value
    encoding = checkbox.value
    for (const input of document.querySelectorAll('#encoding-options input'))
      input.checked = input.value === encoding
    if (changed) {
      stopTransfer()
      setStatus(
        `已选择${encoding === 'raw' ? '不编码（BGRA8）' : encoding.toUpperCase()}，点击获取内存图片应用；内存原图保持 BGRA8。`,
      )
    }
  })
}
$('display-mode').addEventListener('change', () => {
  stopTransfer()
  setStatus(
    $('display-mode').value === 'full'
      ? '整图模式将传输完整 BGRA8 原始字节，内存和带宽开销高；后端已加载时可直接获取。'
      : '已选择瓦片模式；后端已加载时可直接获取 BGRA8。',
  )
})
$('zoom-in').addEventListener('click', () => {
  cancelInteraction()
  view.zoomAt(view.width / 2, view.height / 2, view.zoom * 1.25)
  invalidate()
})
$('zoom-out').addEventListener('click', () => {
  cancelInteraction()
  view.zoomAt(view.width / 2, view.height / 2, view.zoom / 1.25)
  invalidate()
})
$('fit-image').addEventListener('click', () => {
  cancelInteraction()
  view.fit()
  invalidate()
})
$('actual-size').addEventListener('click', () => {
  cancelInteraction()
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
  if (roiDrag) cancelInteraction()
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
  backendSequence++
  backendController?.abort()
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
  image = null
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
  $('load-backend').disabled = true
}
