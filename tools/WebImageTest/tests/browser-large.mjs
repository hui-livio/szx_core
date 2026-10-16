import assert from 'node:assert/strict'
import { createRequire } from 'node:module'
import { mkdir, writeFile } from 'node:fs/promises'
import { fileURLToPath } from 'node:url'
import { resolve } from 'node:path'

const require = createRequire(new URL('../../szx_admin/web/admin/package.json', import.meta.url))
const { chromium } = require('@playwright/test')
const url = process.env.WEB_IMAGE_URL || 'http://127.0.0.1:7380'
const output = fileURLToPath(new URL('../test-results/', import.meta.url))
await mkdir(output, { recursive: true })
const browser = await chromium.launch({
  headless: true,
  args: ['--enable-webgl', '--use-angle=swiftshader', '--enable-unsafe-swiftshader'],
})
const results = []
try {
  for (const fallback of [false, true]) {
    const context = await browser.newContext({
      viewport: { width: 1440, height: 900 },
      deviceScaleFactor: 1.5,
    })
    const page = await context.newPage()
    const errors = [],
      requests = []
    const pending = new Set()
    page.on('pageerror', (error) => errors.push(error.message))
    page.on('request', (request) => {
      const path = new URL(request.url()).pathname
      if (path.startsWith('/api/image')) requests.push(path)
      if (path.startsWith('/api/image/tile/')) pending.add(request)
    })
    for (const name of ['requestfinished', 'requestfailed'])
      page.on(name, (request) => pending.delete(request))
    await page.addInitScript(
      ({ fallback }) => {
        window.textureStats = { live: 0, peak: 0, uploads: 0, maxEdge: 0, draws: 0 }
        const original = HTMLCanvasElement.prototype.getContext
        HTMLCanvasElement.prototype.getContext = function (type, ...args) {
          if (fallback && type === 'webgl2') return null
          return original.call(this, type, ...args)
        }
        if (window.WebGL2RenderingContext) {
          const proto = WebGL2RenderingContext.prototype
          for (const name of ['createTexture', 'deleteTexture', 'texImage2D', 'drawArrays']) {
            const original = proto[name]
            proto[name] = function (...args) {
              if (name === 'texImage2D') {
                window.textureStats.uploads++
                const bitmap = args.at(-1)
                window.textureStats.maxEdge = Math.max(
                  window.textureStats.maxEdge,
                  args[3] || 0,
                  args[4] || 0,
                )
              }
              if (name === 'createTexture') {
                window.textureStats.live++
                window.textureStats.peak = Math.max(
                  window.textureStats.peak,
                  window.textureStats.live,
                )
              }
              if (name === 'deleteTexture') window.textureStats.live--
              if (name === 'drawArrays') window.textureStats.draws++
              return original.apply(this, args)
            }
          }
        }
      },
      { fallback },
    )
    await page.goto(url)
    await page.getByRole('button', { name: '后端加载并解码 BGRA8', exact: true }).click()
    await page.waitForFunction(
      () => document.getElementById('viewport').dataset.backendState === 'ready',
      null,
      { timeout: 120000 },
    )
    await page.getByRole('button', { name: '获取内存图片', exact: true }).click()
    await page.locator('#viewport[data-loaded="true"]').waitFor({ timeout: 30000 })
    const info = await (await context.request.get(`${url}/api/image/info`)).json()
    assert.equal(info.width, 17952)
    assert.equal(info.height, 13200)
    assert.ok(info.tiled)
    assert.ok(info.preview_width <= 2048 && info.preview_height <= 2048)
    await page.waitForFunction(() =>
      document.getElementById('zoom-value').textContent.includes('%'),
    )
    assert.ok(requests.includes('/api/image/preview'))
    assert.ok(!requests.includes('/api/image'), 'must not download the 904 MiB BGRA8 original')
    assert.match(
      await page.locator('#renderer-name').textContent(),
      fallback ? /Canvas2D/ : /WebGL2/,
    )
    await page.screenshot({
      path: resolve(output, `moban-fit-${fallback ? 'canvas' : 'webgl'}.png`),
    })
    await page.getByRole('button', { name: '1:1', exact: true }).click()
    await page.waitForFunction(
      () => Number(document.getElementById('viewport').dataset.tiles) > 0,
      {},
      { timeout: 30000 },
    )
    await page.waitForFunction(() => document.getElementById('zoom-value').textContent === '100.0%')
    await page.waitForTimeout(600)
    // Finish the finite pending tile set before measuring idle behavior.
    for (let i = 0; i < 50 && pending.size; i++) await page.waitForTimeout(100)
    assert.equal(pending.size, 0)
    const count = await page.locator('#viewport').getAttribute('data-tiles')
    assert.ok(Number(count) > 0 && Number(count) <= 48)
    const before = await page.locator('canvas').screenshot()
    await page.locator('#viewport').focus()
    await page.locator('#viewport').press('ArrowRight')
    await page.waitForTimeout(200)
    assert.notDeepEqual(before, await page.locator('canvas').screenshot())
    for (let i = 0; i < 50 && pending.size; i++) await page.waitForTimeout(100)
    await page.waitForTimeout(200)
    const stats = await page.evaluate(() => window.textureStats)
    if (!fallback) {
      assert.ok(stats.maxEdge <= 2048)
      assert.ok(stats.peak <= 49)
      assert.ok(stats.uploads > 1)
      await page.waitForTimeout(300)
      assert.equal((await page.evaluate(() => window.textureStats)).draws, stats.draws)
    }
    const edge = await context.request.get(`${url}/api/image/tile/0/35/25`)
    assert.equal(edge.status(), 200)
    const raw = await edge.body()
    assert.equal(edge.headers()['x-pixel-format'], 'BGRA8')
    assert.equal(edge.headers()['x-image-width'], '32')
    assert.equal(edge.headers()['x-image-height'], '400')
    assert.equal(raw.length, 32 * 400 * 4)
    const invalid = await context.request.get(`${url}/api/image/tile/999/0/0`)
    assert.equal(invalid.status(), 400)
    await page.screenshot({
      path: resolve(output, `moban-detail-${fallback ? 'canvas' : 'webgl'}.png`),
    })
    assert.deepEqual(errors, [])
    results.push({
      renderer: fallback ? 'canvas' : 'webgl',
      width: info.width,
      height: info.height,
      originalRequested: false,
      visibleTiles: Number(count),
      textureStats: stats,
      status: 'passed',
    })
    console.log(
      `PASS moban ${fallback ? 'Canvas2D' : 'WebGL2'}: original=${info.width}x${info.height}, preview=${info.preview_width}x${info.preview_height}, visibleTiles=${count}`,
    )
    if (!fallback) {
      const picker = page.getByRole('combobox', { name: '选择图片', exact: true })
      await picker.selectOption('1_1_01.jpg')
      await page.getByRole('button', { name: '后端加载并解码 BGRA8', exact: true }).click()
      await page.waitForFunction(
        () => document.getElementById('viewport').dataset.backendState === 'ready',
        null,
        { timeout: 120000 },
      )
      await page.getByRole('button', { name: '获取内存图片', exact: true }).click()
      await page.waitForFunction(
        () =>
          document.getElementById('viewport').dataset.name === '1_1_01.jpg' &&
          !document.getElementById('load-image').disabled,
        null,
        { timeout: 120000 },
      )
      assert.match(await page.locator('#image-info').textContent(), /12122 × 10866/)
      await page.getByRole('button', { name: '1:1', exact: true }).click()
      await page.waitForFunction(
        () => Number(document.getElementById('viewport').dataset.tiles) > 0,
      )
      const old = await context.request.get(`${url}/api/image/preview?name=moban.png`)
      assert.equal(
        old.status(),
        409,
        'evicted image must not return the newly selected BGRA8 image',
      )
      await page.screenshot({ path: resolve(output, 'selected-jpg.png') })
      console.log(
        'PASS actual directory selection: 1_1_01.jpg 12122x10866, previous BGRA8 snapshot cannot be confused with current image',
      )
    }
    await context.close()
  }
} finally {
  await writeFile(resolve(output, 'moban-results.json'), JSON.stringify(results, null, 2))
  await browser.close()
}
