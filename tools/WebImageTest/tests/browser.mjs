import assert from 'node:assert/strict'
import { createServer } from 'node:http'
import { readFile, mkdir, writeFile } from 'node:fs/promises'
import { resolve, extname } from 'node:path'
import { fileURLToPath } from 'node:url'
import { createRequire } from 'node:module'
import { deflateSync } from 'node:zlib'

// Reuse admin's test tooling only. The shipped service/page has no Node dependency.
const require = createRequire(new URL('../../szx_admin/web/admin/package.json', import.meta.url))
const { chromium } = require('@playwright/test')
const root = resolve(fileURLToPath(new URL('../web/', import.meta.url)))
const output = fileURLToPath(new URL('../test-results/', import.meta.url))
await mkdir(output, { recursive: true })

function crc32(bytes) {
  let crc = 0xffffffff
  for (const byte of bytes) {
    crc ^= byte
    for (let i = 0; i < 8; i++) crc = (crc >>> 1) ^ (crc & 1 ? 0xedb88320 : 0)
  }
  return (crc ^ 0xffffffff) >>> 0
}
function chunk(name, body) {
  const type = Buffer.from(name)
  const size = Buffer.alloc(4),
    crc = Buffer.alloc(4)
  size.writeUInt32BE(body.length)
  crc.writeUInt32BE(crc32(Buffer.concat([type, body])))
  return Buffer.concat([size, type, body, crc])
}
// Test-only encoder for the committed RGB PPM; never used in production.
const ppm = await readFile(new URL('../assets/sample.ppm', import.meta.url))
const header = ppm.toString('ascii', 0, 30).match(/^P6\n(\d+) (\d+)\n255\n/)
const width = Number(header[1]),
  height = Number(header[2])
const rows = Buffer.alloc((width * 3 + 1) * height)
for (let row = 0; row < height; row++)
  ppm.copy(
    rows,
    row * (width * 3 + 1) + 1,
    header[0].length + row * width * 3,
    header[0].length + (row + 1) * width * 3,
  )
const ihdr = Buffer.alloc(13)
ihdr.writeUInt32BE(width)
ihdr.writeUInt32BE(height, 4)
ihdr[8] = 8
ihdr[9] = 2
const png = Buffer.concat([
  Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]),
  chunk('IHDR', ihdr),
  chunk('IDAT', deflateSync(rows)),
  chunk('IEND', Buffer.alloc(0)),
])
let server
let url = process.env.WEB_IMAGE_URL
if (!url) {
  server = createServer(async (request, response) => {
    try {
      const path = new URL(request.url, 'http://localhost').pathname
      if (path === '/api/images') {
        response.writeHead(200, { 'Content-Type': 'application/json' })
        response.end(
          JSON.stringify({
            default: 'sample.ppm',
            items: [{ name: 'sample.ppm', size_bytes: ppm.length, loadable: true }],
          }),
        )
        return
      }
      if (path === '/api/image/info') {
        response.writeHead(200, { 'Content-Type': 'application/json' })
        response.end(JSON.stringify({ width, height, tiled: false }))
        return
      }
      if (path === '/api/image') {
        response.writeHead(200, { 'Content-Type': 'image/png', 'Content-Length': png.length })
        response.end(png)
        return
      }
      const file = resolve(root, `.${path === '/' ? '/index.html' : path}`)
      if (!file.startsWith(`${root}/`)) {
        response.writeHead(403)
        response.end()
        return
      }
      response.writeHead(200, {
        'Content-Type':
          { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css' }[extname(file)] ||
          'application/octet-stream',
      })
      response.end(await readFile(file))
    } catch {
      response.writeHead(404)
      response.end()
    }
  })
  await new Promise((done) => server.listen(0, '127.0.0.1', done))
  url = `http://127.0.0.1:${server.address().port}`
  console.log('Browser fixture mode: Node static/API fixture, NOT C++ backend verification.')
} else console.log(`Live service mode: ${url}`)

const browser = await chromium.launch({
  headless: true,
  args: ['--enable-webgl', '--use-angle=swiftshader', '--enable-unsafe-swiftshader'],
})
let passed = 0
const results = []
async function run(name, callback, force2d = false, viewport = { width: 1366, height: 900 }) {
  const context = await browser.newContext({ viewport, deviceScaleFactor: 1.5 })
  const page = await context.newPage()
  const errors = []
  page.on('pageerror', (error) => errors.push(error.message))
  await page.addInitScript(
    ({ force2d }) => {
      window.renderStats = { uploads: 0, draws: 0, closes: 0, canvasDraws: 0 }
      const get = HTMLCanvasElement.prototype.getContext
      HTMLCanvasElement.prototype.getContext = function (type, ...args) {
        if (force2d && (type === 'webgl2' || type === 'webgl')) return null
        return get.call(this, type, ...args)
      }
      if (window.WebGL2RenderingContext) {
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
      }
      const draw = CanvasRenderingContext2D.prototype.drawImage
      CanvasRenderingContext2D.prototype.drawImage = function (...args) {
        window.renderStats.canvasDraws++
        return draw.apply(this, args)
      }
      if (window.ImageBitmap) {
        const close = ImageBitmap.prototype.close
        ImageBitmap.prototype.close = function () {
          window.renderStats.closes++
          return close.call(this)
        }
      }
    },
    { force2d },
  )
  try {
    await page.goto(url)
    await callback(page)
    assert.deepEqual(errors, [])
    passed++
    results.push({ name, status: 'passed' })
    console.log(`PASS ${name}`)
  } catch (error) {
    results.push({ name, status: 'failed', error: error.message })
    await page.screenshot({ path: resolve(output, 'failure.png') }).catch(() => {})
    throw error
  } finally {
    await context.close()
  }
}
async function load(page) {
  await page.getByRole('button', { name: '获取内存图片', exact: true }).click()
  await page.locator('#viewport[data-loaded="true"]').waitFor()
  await page.waitForFunction(() => !document.getElementById('load-image').disabled)
  await page.waitForFunction(() => document.getElementById('zoom-value').textContent.includes('%'))
}
async function zoom(page) {
  return Number.parseFloat(await page.locator('#zoom-value').textContent())
}
async function pixel(page, screenshot, x, y) {
  return page.evaluate(
    async ({ bytes, x, y }) => {
      const bitmap = await createImageBitmap(
        new Blob([Uint8Array.from(atob(bytes), (c) => c.charCodeAt(0))], { type: 'image/png' }),
      )
      const canvas = document.createElement('canvas')
      canvas.width = bitmap.width
      canvas.height = bitmap.height
      const ctx = canvas.getContext('2d')
      ctx.drawImage(bitmap, 0, 0)
      bitmap.close()
      return [...ctx.getImageData(Math.floor(x), Math.floor(y), 1, 1).data]
    },
    { bytes: screenshot.toString('base64'), x, y },
  )
}
try {
  await run(
    'GPU texture upload, orientation/color, zoom, pan, reset and idle budget',
    async (page) => {
      await load(page)
      assert.match(await page.locator('#renderer-name').textContent(), /WebGL2/)
      assert.match(await page.locator('#image-info').textContent(), /192 × 128/)
      const initial = await zoom(page)
      const canvas = page.locator('canvas')
      const shot = await canvas.screenshot()
      const bounds = await canvas.boundingBox()
      const factor = 1.5
      const scale = initial / 100
      // Upper-left red marker and upper-right green marker in the committed calibration image.
      const left = bounds.width / 2 - (width * scale) / 2
      const top = bounds.height / 2 - (height * scale) / 2
      const red = await pixel(page, shot, (left + 12 * scale) * factor, (top + 12 * scale) * factor)
      const green = await pixel(
        page,
        shot,
        (left + (width - 12) * scale) * factor,
        (top + 12 * scale) * factor,
      )
      assert.ok(red[0] > 220 && red[1] < 30 && red[2] < 30, `red marker: ${red}`)
      assert.ok(green[1] > 220 && green[0] < 30 && green[2] < 30, `green marker: ${green}`)
      await page.getByRole('button', { name: '放大', exact: true }).click()
      await page.waitForFunction(
        (value) => Number.parseFloat(document.getElementById('zoom-value').textContent) > value,
        initial,
      )
      await canvas.hover()
      await page.mouse.wheel(0, -200)
      await page.waitForTimeout(100)
      const beforePan = await canvas.screenshot()
      const box = await canvas.boundingBox()
      await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2)
      await page.mouse.down()
      await page.mouse.move(box.x + box.width / 2 + 70, box.y + box.height / 2 + 40, { steps: 5 })
      await page.mouse.up()
      const afterPan = await canvas.screenshot()
      assert.notDeepEqual(beforePan, afterPan)
      await page.getByRole('button', { name: '1:1', exact: true }).click()
      await page.waitForFunction(
        () => document.getElementById('zoom-value').textContent === '100.0%',
      )
      await page.locator('#viewport').press('0')
      await page.waitForFunction(
        (value) =>
          Math.abs(Number.parseFloat(document.getElementById('zoom-value').textContent) - value) <
          0.2,
        initial,
      )
      assert.equal((await page.evaluate(() => window.renderStats)).uploads, 1)
      await page.waitForTimeout(150)
      const stats = await page.evaluate(() => window.renderStats)
      await page.waitForTimeout(350)
      assert.equal((await page.evaluate(() => window.renderStats)).draws, stats.draws)
      await page.screenshot({ path: resolve(output, 'webgl-viewer.png') })
    },
  )
  await run(
    'reloading closes the previous ImageBitmap and failures retain the last image',
    async (page) => {
      await load(page)
      await load(page)
      assert.equal((await page.evaluate(() => window.renderStats)).uploads, 2)
      assert.equal((await page.evaluate(() => window.renderStats)).closes, 1)
      await page.route('**/api/image?*', (route) =>
        route.fulfill({ status: 503, body: 'unavailable' }),
      )
      await page.getByRole('button', { name: '获取内存图片', exact: true }).click()
      await page.waitForFunction(() =>
        document.getElementById('status').textContent.includes('503'),
      )
      assert.equal(await page.locator('#empty-state').isHidden(), true)
      assert.equal(await page.getByRole('button', { name: '放大', exact: true }).isEnabled(), true)
      assert.equal((await page.evaluate(() => window.renderStats)).uploads, 2)
    },
  )
  await run(
    'Canvas2D fallback still renders, zooms and remains idle',
    async (page) => {
      await load(page)
      assert.match(await page.locator('#renderer-name').textContent(), /Canvas2D/)
      await page.getByRole('button', { name: '1:1', exact: true }).click()
      await page.waitForFunction(
        () => document.getElementById('zoom-value').textContent === '100.0%',
      )
      await page.waitForTimeout(100)
      const count = (await page.evaluate(() => window.renderStats)).canvasDraws
      assert.ok(count > 0)
      await page.waitForTimeout(250)
      assert.equal((await page.evaluate(() => window.renderStats)).canvasDraws, count)
      await page.screenshot({ path: resolve(output, 'canvas-fallback.png') })
    },
    true,
  )
  await run(
    'narrow high-DPI viewport and keyboard controls',
    async (page) => {
      await load(page)
      assert.equal(await page.locator('body').evaluate((e) => e.scrollWidth <= innerWidth), true)
      const canvas = page.locator('canvas')
      const dimensions = await canvas.evaluate((e) => ({ width: e.width, height: e.height }))
      assert.ok(dimensions.width * dimensions.height <= 16 * 1024 * 1024)
      await page.locator('#viewport').focus()
      await page.locator('#viewport').press('1')
      await page.waitForFunction(
        () => document.getElementById('zoom-value').textContent === '100.0%',
      )
      await page.locator('#viewport').press('+')
      await page.waitForFunction(
        () => document.getElementById('zoom-value').textContent === '125.0%',
      )
      await page.screenshot({ path: resolve(output, 'mobile-viewer.png') })
    },
    false,
    { width: 390, height: 844 },
  )
  await run('WebGL context loss and restoration reupload the retained bitmap', async (page) => {
    await load(page)
    const supported = await page.evaluate(() => {
      const gl = document.querySelector('canvas').getContext('webgl2')
      window.lossExtension = gl.getExtension('WEBGL_lose_context')
      if (!window.lossExtension) return false
      window.lossExtension.loseContext()
      return true
    })
    assert.equal(supported, true)
    await page.waitForFunction(() => document.getElementById('load-image').disabled)
    await page.waitForTimeout(150)
    await page.evaluate(() => window.lossExtension.restoreContext())
    await page.waitForFunction(() => !document.getElementById('load-image').disabled)
    assert.match(await page.locator('#renderer-name').textContent(), /WebGL2/)
    assert.equal((await page.evaluate(() => window.renderStats)).uploads, 2)
    assert.equal(await page.locator('#empty-state').isHidden(), true)
  })
  await run(
    'two-pointer pinch and pointer cancellation release interaction state',
    async (page) => {
      await load(page)
      const initial = await zoom(page)
      const box = await page.locator('#viewport').boundingBox()
      const client = await page.context().newCDPSession(page)
      const a = { x: box.x + box.width / 2 - 50, y: box.y + box.height / 2, id: 1 }
      const b = { x: box.x + box.width / 2 + 50, y: box.y + box.height / 2, id: 2 }
      await client.send('Input.dispatchTouchEvent', { type: 'touchStart', touchPoints: [a, b] })
      await client.send('Input.dispatchTouchEvent', {
        type: 'touchMove',
        touchPoints: [
          { ...a, x: a.x - 35 },
          { ...b, x: b.x + 35 },
        ],
      })
      await page.waitForFunction(
        (value) =>
          Number.parseFloat(document.getElementById('zoom-value').textContent) > value * 1.4,
        initial,
      )
      await client.send('Input.dispatchTouchEvent', { type: 'touchCancel', touchPoints: [] })
      await page.waitForFunction(
        () => !document.getElementById('viewport').classList.contains('dragging'),
      )
      await client.detach()
    },
  )
  await run('invalid payload is reported and never replaces the retained texture', async (page) => {
    await load(page)
    await page.route('**/api/image?*', (route) =>
      route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: '{}',
      }),
    )
    await page.getByRole('button', { name: '获取内存图片', exact: true }).click()
    await page.waitForFunction(() => document.getElementById('status').textContent.includes('PNG'))
    assert.equal((await page.evaluate(() => window.renderStats)).uploads, 1)
    assert.equal(await page.locator('#empty-state').isHidden(), true)
    await page.unroute('**/api/image?*')
    await page.route('**/api/image?*', (route) =>
      route.fulfill({
        status: 200,
        contentType: 'image/png',
        body: 'invalid PNG bytes',
      }),
    )
    await page.getByRole('button', { name: '获取内存图片', exact: true }).click()
    await page.waitForFunction(() =>
      document.getElementById('status').parentElement.classList.contains('error'),
    )
    assert.equal((await page.evaluate(() => window.renderStats)).uploads, 1)
    assert.equal(await page.getByRole('button', { name: '放大', exact: true }).isEnabled(), true)
  })
  console.log(`${passed} browser checks passed.`)
} finally {
  await writeFile(
    resolve(output, 'browser-results.json'),
    JSON.stringify({ mode: server ? 'fixture' : 'live-cpp', results }, null, 2),
  )
  await browser.close()
  if (server) await new Promise((done) => server.close(done))
}
