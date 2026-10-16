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
try {
  const context = await browser.newContext({
    viewport: { width: 1280, height: 800 },
    deviceScaleFactor: 1,
  })
  const page = await context.newPage()
  const requests = [],
    errors = []
  page.on('request', (req) => {
    const path = new URL(req.url()).pathname
    if (path.startsWith('/api/image')) requests.push(new URL(req.url()))
  })
  page.on('pageerror', (error) => errors.push(error.message))
  await page.goto(url)
  await page.waitForFunction(() => !document.getElementById('image-select').disabled)
  await page.getByRole('combobox', { name: '选择图片', exact: true }).selectOption('moban.png')
  await page.getByRole('combobox', { name: '显示模式', exact: true }).selectOption('full')
  const started = performance.now()
  await page.getByRole('button', { name: '获取内存图片', exact: true }).click()
  await page.waitForFunction(
    () => {
      const host = document.getElementById('viewport')
      return (
        (host.dataset.mode === 'full' && host.dataset.name === 'moban.png') ||
        document.getElementById('status').parentElement.classList.contains('error')
      )
    },
    null,
    { timeout: 120000 },
  )
  assert.equal(
    await page.locator('#viewport').getAttribute('data-mode'),
    'full',
    await page.locator('#status').textContent(),
  )
  assert.equal(await page.locator('#viewport').getAttribute('data-tiles'), '0')
  assert.ok(
    requests.some(
      (req) => req.pathname === '/api/image' && req.searchParams.get('name') === 'moban.png',
    ),
  )
  assert.ok(
    !requests.some((req) => req.pathname.includes('/tile/') || req.pathname.includes('/preview')),
  )
  assert.match(await page.locator('#image-info').textContent(), /17952 × 13200/)
  await page.getByRole('button', { name: '1:1', exact: true }).click()
  await page.waitForFunction(() => document.getElementById('zoom-value').textContent === '100.0%')
  const canvas = page.locator('canvas')
  const before = await canvas.screenshot()
  await page.locator('#viewport').focus()
  await page.locator('#viewport').press('ArrowRight')
  assert.notDeepEqual(before, await canvas.screenshot())
  await page.screenshot({ path: resolve(output, 'moban-full-image.png') })
  const result = {
    width: 17952,
    height: 13200,
    mode: 'full',
    renderer: await page.locator('#renderer-name').textContent(),
    previewOrTileRequests: 0,
    elapsedMs: Math.round(performance.now() - started),
    status: 'passed',
  }
  assert.deepEqual(errors, [])
  await writeFile(resolve(output, 'moban-full-results.json'), JSON.stringify(result, null, 2))
  console.log('PASS actual moban non-tiled full-image path:', JSON.stringify(result))
  await context.close()
} finally {
  await browser.close()
}
