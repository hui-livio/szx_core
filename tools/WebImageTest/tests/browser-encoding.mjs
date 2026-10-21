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
const directory = await mkdtemp(join(tmpdir(), 'szx-encoding-browser-'))
const output = fileURLToPath(new URL('../test-results/', import.meta.url))
const binary =
  process.env.WEB_IMAGE_BINARY ||
  fileURLToPath(new URL('../../../build/bin/szx_web_image_test', import.meta.url))
await mkdir(output, { recursive: true })
async function fixture(name, width, height) {
  const pixels = Buffer.alloc(width * height * 3)
  for (let i = 0; i < pixels.length; i += 3) {
    pixels[i] = 211
    pixels[i + 1] = 83
    pixels[i + 2] = 17
  }
  await writeFile(
    join(directory, name),
    Buffer.concat([Buffer.from(`P6\n${width} ${height}\n255\n`), pixels]),
  )
}
await fixture('small.ppm', 192, 128)
await fixture('large.ppm', 2305, 1025)
const service = spawn(
  binary,
  ['--images-dir', directory, '--image', join(directory, 'small.ppm'), '--port', '0'],
  {
    stdio: ['ignore', 'pipe', 'pipe'],
  },
)
let browser
try {
  let logs = ''
  service.stderr.on('data', (chunk) => {
    logs += chunk
  })
  const url = await new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error(`Service startup timeout: ${logs}`)), 15000)
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
    await fixture('small.ppm', 192, 128)
    const context = await browser.newContext({ viewport: { width: 1366, height: 900 } })
    const page = await context.newPage()
    const errors = []
    page.on('pageerror', (error) => errors.push(error.message))
    await page.addInitScript(
      ({ fallback }) => {
        if (fallback) {
          const original = HTMLCanvasElement.prototype.getContext
          HTMLCanvasElement.prototype.getContext = function (type, ...args) {
            return type === 'webgl2' ? null : original.call(this, type, ...args)
          }
        }
        const decode = window.createImageBitmap
        window.createImageBitmap = async (...args) => {
          window.decoding = true
          if (window.pauseDecode)
            await new Promise((resolve) => {
              window.resumeDecode = resolve
            })
          const bitmap = await decode(...args)
          window.decoding = false
          return bitmap
        }
      },
      { fallback },
    )
    await page.goto(url)
    await page.waitForFunction(() => !document.getElementById('image-select').disabled)
    await page.locator('#image-select').selectOption('small.ppm')
    await page.locator('#display-mode').selectOption('full')
    await page.locator('#load-backend').click()
    await page.waitForFunction(
      () => document.getElementById('viewport').dataset.backendState === 'ready',
    )
    const snapshot = await (
      await context.request.get(`${url}/api/image/info?name=small.ppm`)
    ).json()
    await rm(join(directory, 'small.ppm'))
    let rawScreenshot
    for (const encoding of ['raw', 'png', 'jpg']) {
      await page.locator(`#encoding-${encoding}`).click()
      assert.equal(await page.locator('#encoding-options input:checked').count(), 1)
      assert.equal(await page.locator(`#encoding-${encoding}`).isChecked(), true)
      const responsePromise = page.waitForResponse(
        (response) => new URL(response.url()).pathname === '/api/image',
      )
      await page.locator('#load-image').click()
      const response = await responsePromise
      assert.equal(
        new URL(response.url()).searchParams.get('encoding'),
        encoding === 'raw' ? null : encoding,
      )
      assert.equal(
        response.headers()['content-type'],
        { raw: 'application/octet-stream', png: 'image/png', jpg: 'image/jpeg' }[encoding],
      )
      assert.equal(response.headers()['x-image-id'], snapshot.id)
      const bytes = await response.body()
      if (encoding === 'png')
        assert.deepEqual([...bytes.subarray(0, 8)], [137, 80, 78, 71, 13, 10, 26, 10])
      if (encoding === 'jpg') assert.deepEqual([...bytes.subarray(0, 2)], [255, 216])
      await page.waitForFunction(
        (encoding) =>
          document.getElementById('viewport').dataset.encoding === encoding &&
          !document.getElementById('load-image').disabled,
        encoding,
      )
      assert.ok(Number(await page.locator('#transfer-time').getAttribute('data-ms')) >= 0)
      await page.locator('#actual-size').click()
      const screenshot = await page.locator('#image-canvas').screenshot()
      if (encoding === 'raw') rawScreenshot = screenshot
      if (encoding === 'png')
        assert.deepEqual(screenshot, rawScreenshot, 'PNG colours and origin match BGRA rendering')
      const raw = await context.request.get(`${url}/api/image?name=small.ppm&id=${snapshot.id}`)
      assert.equal(raw.status(), 200)
      assert.deepEqual([...(await raw.body()).subarray(0, 4)], [17, 83, 211, 255])
    }
    // Toggling the checked option must not leave all three unchecked.
    await page.locator('#encoding-jpg').click()
    assert.equal(await page.locator('#encoding-options input:checked').count(), 1)
    assert.equal(await page.locator('#encoding-jpg').isChecked(), true)
    await page.screenshot({ path: join(output, `encoding-${fallback ? 'canvas' : 'webgl'}.png`) })
    // Reception timing is visible while browser decoding is still paused; switching cancels stale publication.
    await page.locator('#encoding-png').click()
    await page.evaluate(() => {
      window.pauseDecode = true
    })
    await page.locator('#load-image').click()
    await page.waitForFunction(() => window.decoding)
    assert.ok(Number(await page.locator('#transfer-time').getAttribute('data-ms')) >= 0)
    assert.equal(await page.locator('#load-image').isDisabled(), true)
    await page.locator('#encoding-raw').click()
    await page.evaluate(() => {
      window.pauseDecode = false
      window.resumeDecode()
    })
    await page.waitForFunction(() => !window.decoding)
    assert.equal(await page.locator('#viewport').getAttribute('data-encoding'), 'jpg')
    assert.equal(await page.locator('#transfer-time').getAttribute('data-ms'), null)
    for (const encoding of ['png', 'jpg']) {
      await page.locator('#image-select').selectOption('large.ppm')
      await page.locator('#display-mode').selectOption('tiles')
      await page.locator(`#encoding-${encoding}`).click()
      await page.locator('#load-backend').click()
      await page.waitForFunction(
        () => document.getElementById('viewport').dataset.backendState === 'ready',
      )
      const previewPromise = page.waitForResponse(
        (response) => new URL(response.url()).pathname === '/api/image/preview',
      )
      await page.locator('#load-image').click()
      const preview = await previewPromise
      assert.equal(new URL(preview.url()).searchParams.get('encoding'), encoding)
      await page.waitForFunction(() => !document.getElementById('load-image').disabled)
      const tilePromise = page.waitForResponse((response) =>
        new URL(response.url()).pathname.includes('/api/image/tile/'),
      )
      await page.locator('#actual-size').click()
      const tile = await tilePromise
      assert.equal(new URL(tile.url()).searchParams.get('encoding'), encoding)
      assert.equal(tile.headers()['content-type'], encoding === 'png' ? 'image/png' : 'image/jpeg')
      await page.waitForFunction(
        () => Number(document.getElementById('viewport').dataset.tiles) > 0,
      )
    }
    await page.setViewportSize({ width: 390, height: 844 })
    await page.locator('#encoding-options').scrollIntoViewIfNeeded()
    assert.equal(
      await page.locator('body').evaluate((body) => body.scrollWidth <= innerWidth),
      true,
    )
    await page.screenshot({
      path: join(output, `encoding-mobile-${fallback ? 'canvas' : 'webgl'}.png`),
    })
    assert.deepEqual(errors, [])
    console.log(
      `PASS encoding selection, BGRA snapshot preservation, full/preview/tile transfer, timing and cancellation (${fallback ? 'Canvas2D' : 'WebGL2'})`,
    )
    await context.close()
  }
} finally {
  await browser?.close()
  if (service.exitCode === null && service.signalCode === null) {
    const stopped = once(service, 'exit')
    service.kill('SIGTERM')
    await stopped
  }
  await rm(directory, { recursive: true, force: true })
}
