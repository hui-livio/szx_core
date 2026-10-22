import assert from 'node:assert/strict'
import { createRequire } from 'node:module'
import { mkdtemp, mkdir, writeFile, rm } from 'node:fs/promises'
import { tmpdir } from 'node:os'
import { join } from 'node:path'
import { fileURLToPath } from 'node:url'
import { spawn } from 'node:child_process'
import { once } from 'node:events'

const require = createRequire(new URL('../../szx_admin/web/admin/package.json', import.meta.url))
const { chromium } = require('@playwright/test')
const directory = await mkdtemp(join(tmpdir(), 'szx-live-browser-'))
const output = fileURLToPath(new URL('../test-results/', import.meta.url))
const binary =
  process.env.WEB_IMAGE_BINARY ||
  fileURLToPath(new URL('../../../build/bin/szx_web_image_test', import.meta.url))
await mkdir(output, { recursive: true })
const service = spawn(
  binary,
  ['--images-dir', directory, '--image', join(directory, 'small.ppm'), '--port', '0'],
  { stdio: ['ignore', 'pipe', 'pipe'] },
)
let browser
const results = []
try {
  let logs = ''
  service.stderr.on('data', (chunk) => {
    logs += chunk
  })
  const url = await new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error(`Startup timeout: ${logs}`)), 15000)
    service.once('error', (error) => {
      clearTimeout(timer)
      reject(error)
    })
    service.once('exit', (code) => {
      clearTimeout(timer)
      reject(new Error(`Service exited ${code}: ${logs}`))
    })
    service.stdout.on('data', (chunk) => {
      logs += chunk
      const match = logs.match(/http:\/\/127\.0\.0\.1:\d+/)
      if (match) {
        clearTimeout(timer)
        resolve(match[0])
      }
    })
  })
  browser = await chromium.launch({
    headless: true,
    args: ['--enable-webgl', '--use-angle=swiftshader', '--enable-unsafe-swiftshader'],
  })
  for (const fallback of [false, true]) {
    await rm(join(directory, 'small.ppm'), { force: true })
    const context = await browser.newContext({ viewport: { width: 1366, height: 1000 } })
    const page = await context.newPage()
    const errors = [],
      requests = []
    page.on('pageerror', (error) => errors.push(error.message))
    page.on('request', (request) => {
      if (new URL(request.url()).pathname === '/api/live/frame') requests.push(Date.now())
    })
    await page.addInitScript(
      ({ fallback }) => {
        window.liveRenderStats = { allocations: 0, updates: 0, decodes: 0 }
        window.originalBitmap = window.createImageBitmap
        window.createImageBitmap = (...args) => {
          window.liveRenderStats.decodes++
          return window.originalBitmap(...args)
        }
        if (fallback) {
          const original = HTMLCanvasElement.prototype.getContext
          HTMLCanvasElement.prototype.getContext = function (type, ...args) {
            return type === 'webgl2' ? null : original.call(this, type, ...args)
          }
        } else {
          for (const [method, key] of [
            ['texImage2D', 'allocations'],
            ['texSubImage2D', 'updates'],
          ]) {
            const original = WebGL2RenderingContext.prototype[method]
            WebGL2RenderingContext.prototype[method] = function (...args) {
              window.liveRenderStats[key]++
              return original.apply(this, args)
            }
          }
        }
      },
      { fallback },
    )
    await page.goto(url)
    await page.waitForFunction(() =>
      document.getElementById('image-select').textContent.includes('没有可加载'),
    )
    assert.equal(await page.locator('#start-live').isEnabled(), true)
    const responsePromise = page.waitForResponse('**/api/live/frame')
    await page.locator('#start-live').click()
    const response = await responsePromise
    assert.equal(response.status(), 200)
    assert.equal(response.headers()['x-image-width'], '2448')
    assert.equal(response.headers()['x-image-height'], '2048')
    assert.equal(response.headers()['x-image-byte-length'], '20054016')
    assert.equal(response.headers()['x-pixel-format'], 'BGRA8')
    assert.equal(response.headers()['content-type'], 'application/octet-stream')
    await page.waitForFunction(
      () => Number(document.getElementById('viewport').dataset.liveFrames) >= 3,
    )
    assert.equal(await page.locator('#viewport').getAttribute('data-mode'), 'live')
    assert.equal(await page.locator('#encoding-png').isDisabled(), true)
    assert.equal(await page.locator('#load-backend').isDisabled(), true)
    assert.match(await page.locator('#image-info').textContent(), /2448 × 2048/)
    const stats = await page.evaluate(() => window.liveRenderStats)
    assert.equal(stats.decodes, 0)
    if (!fallback) {
      assert.equal(stats.allocations, 1)
      assert.ok(stats.updates >= 2)
    }

    // Use a low target to verify pacing independently of machine throughput.
    await page.locator('#live-fps').fill('1')
    await page.locator('#live-fps').blur()
    const before = requests.length
    await page.waitForTimeout(2300)
    const lowFpsRequests = requests.length - before
    assert.ok(
      lowFpsRequests >= 1 && lowFpsRequests <= 3,
      `1 FPS requested ${lowFpsRequests} frames`,
    )
    assert.match(await page.locator('#live-stats').textContent(), /目标 1 FPS/)
    await page.locator('#stop-live').click()
    const stoppedRequests = requests.length
    await page.waitForTimeout(1100)
    assert.equal(requests.length, stoppedRequests)
    assert.equal(await page.locator('#empty-state').isHidden(), true)
    const screenshot = await page.locator('#image-canvas').screenshot()
    await writeFile(join(output, `live-${fallback ? 'canvas' : 'webgl'}.png`), screenshot)
    const colours = await page.evaluate(async (base64) => {
      const bytes = Uint8Array.from(atob(base64), (c) => c.charCodeAt(0))
      const bitmap = await window.originalBitmap(new Blob([bytes], { type: 'image/png' }))
      const canvas = new OffscreenCanvas(bitmap.width, bitmap.height),
        ctx = canvas.getContext('2d')
      ctx.drawImage(bitmap, 0, 0)
      const data = ctx.getImageData(0, 0, canvas.width, canvas.height).data
      let black = 0,
        white = 0
      for (let i = 0; i < data.length; i += 4) {
        if (data[i] < 2 && data[i + 1] < 2 && data[i + 2] < 2) black++
        if (data[i] > 180 && data[i + 1] > 180 && data[i + 2] > 180) white++
      }
      bitmap.close()
      return { black, white }
    }, screenshot.toString('base64'))
    assert.ok(colours.black > 10000 && colours.white > 20, JSON.stringify(colours))

    await page.locator('#live-fps').fill('10')
    await page.locator('#start-live').click()
    await page.waitForFunction(
      () => Number(document.getElementById('viewport').dataset.liveFrames) >= 2,
    )
    await page.evaluate(() => {
      Object.defineProperty(document, 'hidden', { configurable: true, value: true })
      document.dispatchEvent(new Event('visibilitychange'))
    })
    const hiddenRequests = requests.length
    await page.waitForTimeout(400)
    assert.equal(requests.length, hiddenRequests)
    await page.evaluate(() => {
      Object.defineProperty(document, 'hidden', { configurable: true, value: false })
      document.dispatchEvent(new Event('visibilitychange'))
    })
    await page.waitForFunction(
      (previous) => Number(document.getElementById('viewport').dataset.liveFrames) > previous,
      Number(await page.locator('#viewport').getAttribute('data-live-frames')),
    )
    if (!fallback) {
      await page.evaluate(() => {
        window.liveContextLoss = document
          .getElementById('image-canvas')
          .getContext('webgl2')
          .getExtension('WEBGL_lose_context')
        window.liveContextLoss.loseContext()
      })
      await page.waitForFunction(() =>
        document.getElementById('status').textContent.includes('上下文已丢失'),
      )
      const lostRequests = requests.length
      const previousFrames = Number(
        await page.locator('#viewport').getAttribute('data-live-frames'),
      )
      await page.waitForTimeout(200)
      assert.equal(requests.length, lostRequests)
      await page.evaluate(() => window.liveContextLoss.restoreContext())
      await page.waitForFunction(
        (previous) => Number(document.getElementById('viewport').dataset.liveFrames) > previous,
        previousFrames,
      )
    }
    await page.route('**/api/live/frame', (route) =>
      route.fulfill({ status: 503, contentType: 'application/json', body: '{"code":1007}' }),
    )
    await page.waitForFunction(() => document.getElementById('viewport').dataset.live === 'stopped')
    assert.match(await page.locator('#status').textContent(), /内存不足/)
    await page.unroute('**/api/live/frame')
    await page.locator('#start-live').click()
    await page.waitForFunction(
      () => Number(document.getElementById('viewport').dataset.liveFrames) >= 2,
    )
    await page.locator('#stop-live').click()

    // Return to the existing file workflow after a live session.
    await writeFile(
      join(directory, 'small.ppm'),
      Buffer.concat([Buffer.from('P6\n2 1\n255\n'), Buffer.from([211, 83, 17, 211, 83, 17])]),
    )
    await page.locator('#refresh-images').click()
    await page.waitForFunction(() => !document.getElementById('image-select').disabled)
    await page.locator('#load-backend').click()
    await page.waitForFunction(
      () => document.getElementById('viewport').dataset.backendState === 'ready',
    )
    await page.locator('#load-image').click()
    await page.waitForFunction(() => document.getElementById('viewport').dataset.mode === 'tiles')
    assert.match(await page.locator('#image-info').textContent(), /small.ppm/)
    assert.deepEqual(errors, [])
    results.push({
      renderer: fallback ? 'Canvas2D' : 'WebGL2',
      dimensions: [2448, 2048],
      lowFpsRequests,
      colours,
      stats,
    })
    await context.close()
  }
  console.log(JSON.stringify(results, null, 2))
} finally {
  await writeFile(join(output, 'live-browser-results.json'), JSON.stringify(results, null, 2))
  await browser?.close()
  if (service.exitCode === null && service.signalCode === null) {
    const stopped = once(service, 'exit')
    service.kill('SIGTERM')
    await stopped
  }
  await rm(directory, { recursive: true, force: true })
}
