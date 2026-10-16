import assert from 'node:assert/strict'
import { createServer } from 'node:http'
import { readFile, mkdir, writeFile } from 'node:fs/promises'
import { resolve, extname } from 'node:path'
import { fileURLToPath } from 'node:url'
import { createRequire } from 'node:module'
const require = createRequire(new URL('../../szx_admin/web/admin/package.json', import.meta.url))
const { chromium } = require('@playwright/test')
const root = resolve(fileURLToPath(new URL('../web/', import.meta.url)))
const output = fileURLToPath(new URL('../test-results/', import.meta.url))
await mkdir(output, { recursive: true })
const ppm = await readFile(new URL('../assets/sample.ppm', import.meta.url))
const header = ppm.toString('ascii', 0, 30).match(/^P6\n(\d+) (\d+)\n255\n/)
const width = Number(header[1]),
  height = Number(header[2])
const raw = new Uint8Array(width * height * 4)
for (let p = 0; p < width * height; p++) {
  raw[p * 4] = ppm[header[0].length + p * 3 + 2]
  raw[p * 4 + 1] = ppm[header[0].length + p * 3 + 1]
  raw[p * 4 + 2] = ppm[header[0].length + p * 3]
  raw[p * 4 + 3] = 255
}
const info = {
  state: 'ready',
  id: 'fixture-bgra',
  width,
  height,
  format: 'BGRA8',
  channels: 4,
  depth_bits: 8,
  stride: width * 4,
  byte_length: raw.length,
  content_type: 'application/octet-stream',
  origin: 'top-left',
  alpha: 'straight',
  preview_width: width,
  preview_height: height,
  preview_byte_length: raw.length,
  tile_size: 512,
  max_level: 0,
  tiled: false,
}
let server,
  url = process.env.WEB_IMAGE_URL
if (!url) {
  let loaded = false
  server = createServer(async (req, res) => {
    try {
      const path = new URL(req.url, 'http://localhost').pathname
      const json = (data, status = 200) => {
        res.writeHead(status, { 'content-type': 'application/json' })
        res.end(JSON.stringify(data))
      }
      if (path === '/api/images') {
        json({
          default: 'sample.ppm',
          items: [{ name: 'sample.ppm', size_bytes: ppm.length, loadable: true }],
        })
        return
      }
      if (path === '/api/image/load' && req.method === 'POST') {
        loaded = true
        json(info)
        return
      }
      if (path === '/api/image/load' || path === '/api/image/info') {
        json(loaded ? info : { code: 1015 }, loaded ? 200 : 409)
        return
      }
      if (path === '/api/image') {
        if (!loaded) {
          json({ code: 1015 }, 409)
          return
        }
        res.writeHead(200, {
          'content-type': 'application/octet-stream',
          'content-length': raw.length,
          'x-pixel-format': 'BGRA8',
          'x-image-width': width,
          'x-image-height': height,
          'x-image-stride': width * 4,
          'x-image-byte-length': raw.length,
          'x-image-origin': 'top-left',
          'x-image-alpha': 'straight',
          'x-image-id': info.id,
        })
        res.end(raw)
        return
      }
      const file = resolve(root, `.${path === '/' ? '/index.html' : path}`)
      if (!file.startsWith(`${root}/`)) {
        res.writeHead(403)
        res.end()
        return
      }
      const bytes = await readFile(file)
      res.writeHead(200, {
        'content-type':
          { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css' }[extname(file)] ||
          'application/octet-stream',
      })
      res.end(bytes)
    } catch {
      res.writeHead(404)
      res.end()
    }
  })
  await new Promise((done) => server.listen(0, '127.0.0.1', done))
  url = `http://127.0.0.1:${server.address().port}`
  console.log('Raw BGRA8 fixture mode (not C++ verification).')
}
const browser = await chromium.launch({
  headless: true,
  args: ['--enable-webgl', '--use-angle=swiftshader', '--enable-unsafe-swiftshader'],
})
const results = []
async function run(name, fn, fallback = false, viewport = { width: 1366, height: 900 }) {
  const context = await browser.newContext({ viewport, deviceScaleFactor: 1.5 })
  const page = await context.newPage()
  const errors = []
  page.on('pageerror', (error) => errors.push(error.message))
  await page.addInitScript(
    ({ fallback }) => {
      window.renderStats = { uploads: 0, draws: 0, canvasDraws: 0, decodes: 0 }
      window.createImageBitmap = () => {
        window.renderStats.decodes++
        throw new Error('App must not decode image files')
      }
      const get = HTMLCanvasElement.prototype.getContext
      HTMLCanvasElement.prototype.getContext = function (type, ...args) {
        if (fallback && type === 'webgl2') return null
        return get.call(this, type, ...args)
      }
      for (const [name, metric] of [
        ['texImage2D', 'uploads'],
        ['drawArrays', 'draws'],
      ]) {
        const original = WebGL2RenderingContext.prototype[name]
        WebGL2RenderingContext.prototype[name] = function (...args) {
          window.renderStats[metric]++
          return original.apply(this, args)
        }
      }
      const put = CanvasRenderingContext2D.prototype.putImageData
      CanvasRenderingContext2D.prototype.putImageData = function (...args) {
        window.renderStats.canvasDraws++
        return put.apply(this, args)
      }
    },
    { fallback },
  )
  try {
    await page.goto(url)
    await fn(page)
    assert.deepEqual(errors, [])
    results.push({ name, status: 'passed' })
    console.log(`PASS ${name}`)
  } catch (error) {
    await page.screenshot({ path: resolve(output, 'failure.png') }).catch(() => {})
    throw error
  } finally {
    await context.close()
  }
}
async function prepare(page) {
  await page.waitForFunction(() => !document.getElementById('image-select').disabled)
  assert.equal(
    await page.getByRole('button', { name: '获取内存图片', exact: true }).isDisabled(),
    true,
  )
  await page.getByRole('button', { name: '后端加载并解码 BGRA8', exact: true }).click()
  await page.waitForFunction(
    () => document.getElementById('viewport').dataset.backendState === 'ready',
  )
  assert.equal(
    await page.locator('#viewport').getAttribute('data-loaded'),
    null,
    'backend preparation must not fetch pixel body',
  )
}
async function fetchImage(page) {
  await page.getByRole('button', { name: '获取内存图片', exact: true }).click()
  await page.waitForFunction(
    () =>
      document.getElementById('viewport').dataset.format === 'BGRA8' &&
      !document.getElementById('load-image').disabled,
  )
  await page.waitForFunction(() => document.getElementById('zoom-value').textContent.includes('%'))
}
async function pixel(page, screenshot, x, y) {
  return page.evaluate(
    async ({ bytes, x, y }) => {
      const blob = new Blob([Uint8Array.from(atob(bytes), (c) => c.charCodeAt(0))], {
        type: 'image/png',
      })
      const url = URL.createObjectURL(blob)
      const image = new Image()
      await new Promise((resolve, reject) => {
        image.onload = resolve
        image.onerror = reject
        image.src = url
      })
      const canvas = document.createElement('canvas')
      canvas.width = image.width
      canvas.height = image.height
      const ctx = canvas.getContext('2d')
      ctx.drawImage(image, 0, 0)
      URL.revokeObjectURL(url)
      return [...ctx.getImageData(Math.floor(x), Math.floor(y), 1, 1).data]
    },
    { bytes: screenshot.toString('base64'), x, y },
  )
}
try {
  for (const fallback of [false, true])
    await run(
      `BGRA8 channel/origin correctness and no browser decode (${fallback ? 'Canvas2D' : 'WebGL2'})`,
      async (page) => {
        await prepare(page)
        await fetchImage(page)
        assert.match(
          await page.locator('#renderer-name').textContent(),
          fallback ? /Canvas2D/ : /WebGL2/,
        )
        const zoom = Number.parseFloat(await page.locator('#zoom-value').textContent()) / 100
        const canvas = page.locator('canvas'),
          bounds = await canvas.boundingBox(),
          shot = await canvas.screenshot()
        const left = bounds.width / 2 - (width * zoom) / 2,
          top = bounds.height / 2 - (height * zoom) / 2
        const red = await pixel(page, shot, (left + 12 * zoom) * 1.5, (top + 12 * zoom) * 1.5)
        const green = await pixel(
          page,
          shot,
          (left + (width - 12) * zoom) * 1.5,
          (top + 12 * zoom) * 1.5,
        )
        assert.ok(red[0] > 220 && red[1] < 30 && red[2] < 30, `red BGRA swizzle: ${red}`)
        assert.ok(green[1] > 220 && green[0] < 30 && green[2] < 30, `green: ${green}`)
        await page.getByRole('button', { name: '1:1', exact: true }).click()
        await page.waitForFunction(
          () => document.getElementById('zoom-value').textContent === '100.0%',
        )
        const before = await canvas.screenshot()
        await page.locator('#viewport').focus()
        await page.locator('#viewport').press('ArrowRight')
        assert.notDeepEqual(before, await canvas.screenshot())
        await page.locator('#viewport').press('+')
        await page.waitForFunction(
          () => document.getElementById('zoom-value').textContent === '125.0%',
        )
        await page.locator('#viewport').press('0')
        await page.waitForTimeout(100)
        const stats = await page.evaluate(() => window.renderStats)
        assert.equal(stats.decodes, 0)
        if (!fallback) assert.equal(stats.uploads, 1)
        await page.waitForTimeout(250)
        const now = await page.evaluate(() => window.renderStats)
        assert.equal(now.draws, stats.draws)
        assert.equal(now.canvasDraws, stats.canvasDraws)
        await page.screenshot({
          path: resolve(output, fallback ? 'canvas-fallback.png' : 'webgl-viewer.png'),
        })
      },
      fallback,
    )
  for (const fallback of [false, true])
    await run(
      `straight-alpha BGRA8 composites correctly (${fallback ? 'Canvas2D' : 'WebGL2'})`,
      async (page) => {
        const alphaInfo = {
          ...info,
          id: 'alpha',
          width: 1,
          height: 1,
          stride: 4,
          byte_length: 4,
          preview_width: 1,
          preview_height: 1,
          preview_byte_length: 4,
        }
        await page.route('**/api/image/load?*', (route) =>
          route.fulfill({
            status: 200,
            contentType: 'application/json',
            body: JSON.stringify(alphaInfo),
          }),
        )
        await page.route('**/api/image?*', (route) =>
          route.fulfill({
            status: 200,
            body: Buffer.from([17, 83, 211, 137]),
            headers: {
              'content-type': 'application/octet-stream',
              'x-pixel-format': 'BGRA8',
              'x-image-id': 'alpha',
              'x-image-width': '1',
              'x-image-height': '1',
              'x-image-stride': '4',
              'x-image-byte-length': '4',
              'x-image-origin': 'top-left',
              'x-image-alpha': 'straight',
            },
          }),
        )
        await prepare(page)
        await fetchImage(page)
        const canvas = page.locator('canvas'),
          box = await canvas.boundingBox()
        const color = await pixel(
          page,
          await canvas.screenshot(),
          (box.width * 1.5) / 2,
          (box.height * 1.5) / 2,
        )
        for (let channel = 0; channel < 3; channel++)
          assert.ok(Math.abs(color[channel] - [118, 51, 21][channel]) <= 2, `alpha pixel ${color}`)
        assert.equal(color[3], 255)
        assert.equal((await page.evaluate(() => window.renderStats)).decodes, 0)
      },
      fallback,
    )
  await run('request failure preserves previous raw image; no new texture upload', async (page) => {
    await prepare(page)
    await fetchImage(page)
    await page.route('**/api/image?*', (route) =>
      route.fulfill({ status: 503, contentType: 'application/json', body: '{"code":1007}' }),
    )
    await page.getByRole('button', { name: '获取内存图片', exact: true }).click()
    await page.waitForFunction(() =>
      document.getElementById('status').parentElement.classList.contains('error'),
    )
    assert.equal(await page.locator('#empty-state').isHidden(), true)
    assert.equal((await page.evaluate(() => window.renderStats)).uploads, 1)
  })
  await run(
    'context restore reuploads retained BGRA8 without backend decode/load',
    async (page) => {
      await prepare(page)
      await fetchImage(page)
      const requests = []
      page.on('request', (req) => requests.push(new URL(req.url()).pathname))
      await page.evaluate(() => {
        window.lossExtension = document
          .querySelector('canvas')
          .getContext('webgl2')
          .getExtension('WEBGL_lose_context')
        window.lossExtension.loseContext()
      })
      await page.waitForFunction(() => document.getElementById('load-image').disabled)
      await page.waitForTimeout(150)
      await page.evaluate(() => window.lossExtension.restoreContext())
      await page.waitForFunction(() => !document.getElementById('load-image').disabled)
      assert.equal((await page.evaluate(() => window.renderStats)).uploads, 2)
      assert.equal((await page.evaluate(() => window.renderStats)).decodes, 0)
      assert.equal(requests.filter((path) => path.startsWith('/api/image')).length, 0)
    },
  )
  await run(
    'narrow viewport and two-pointer pinch remain usable',
    async (page) => {
      await prepare(page)
      await fetchImage(page)
      assert.equal(await page.locator('body').evaluate((e) => e.scrollWidth <= innerWidth), true)
      const initial = Number.parseFloat(await page.locator('#zoom-value').textContent())
      const rect = await page.locator('#viewport').boundingBox(),
        client = await page.context().newCDPSession(page)
      const a = { x: rect.x + rect.width / 2 - 30, y: rect.y + rect.height / 2, id: 1 },
        b = { ...a, x: a.x + 60, id: 2 }
      await client.send('Input.dispatchTouchEvent', { type: 'touchStart', touchPoints: [a, b] })
      await client.send('Input.dispatchTouchEvent', {
        type: 'touchMove',
        touchPoints: [
          { ...a, x: a.x - 20 },
          { ...b, x: b.x + 20 },
        ],
      })
      await page.waitForFunction(
        (value) =>
          Number.parseFloat(document.getElementById('zoom-value').textContent) > value * 1.3,
        initial,
      )
      await client.send('Input.dispatchTouchEvent', { type: 'touchCancel', touchPoints: [] })
      await client.detach()
      await page.screenshot({ path: resolve(output, 'mobile-viewer.png') })
    },
    false,
    { width: 390, height: 844 },
  )
  console.log(`${results.length} raw browser checks passed.`)
} finally {
  await writeFile(
    resolve(output, 'browser-results.json'),
    JSON.stringify({ mode: server ? 'raw-fixture' : 'live-cpp', results }, null, 2),
  )
  await browser.close()
  if (server) await new Promise((done) => server.close(done))
}
