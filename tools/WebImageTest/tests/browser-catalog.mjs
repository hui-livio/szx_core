import assert from 'node:assert/strict'
import { createRequire } from 'node:module'
import { mkdtemp, mkdir, writeFile, rm } from 'node:fs/promises'
import { tmpdir } from 'node:os'
import { join, resolve } from 'node:path'
import { fileURLToPath } from 'node:url'
import { spawn } from 'node:child_process'
import { once } from 'node:events'

const require = createRequire(new URL('../../szx_admin/web/admin/package.json', import.meta.url))
const { chromium } = require('@playwright/test')
const directory = await mkdtemp(join(tmpdir(), 'szx-catalog-browser-'))
const output = fileURLToPath(new URL('../test-results/', import.meta.url))
const binary =
  process.env.WEB_IMAGE_BINARY ||
  fileURLToPath(new URL('../../../build-web-image/bin/szx_web_image_test', import.meta.url))
await mkdir(output, { recursive: true })
async function ppm(name, width, height, color) {
  const body = Buffer.alloc(width * height * 3)
  for (let i = 0; i < body.length; i += 3) {
    body[i] = color[0]
    body[i + 1] = color[1]
    body[i + 2] = color[2]
  }
  await writeFile(
    join(directory, name),
    Buffer.concat([Buffer.from(`P6\n${width} ${height}\n255\n`), body]),
  )
}
await ppm('a.ppm', 192, 128, [220, 25, 15])
await ppm('蓝色 图片 #2.PPM', 96, 80, [15, 25, 220])
await ppm('large.ppm', 2305, 1025, [10, 105, 40])
await writeFile(join(directory, 'bad.png'), 'invalid image')
await writeFile(join(directory, 'ignore.txt'), 'not listed')
const service = spawn(
  binary,
  ['--images-dir', directory, '--image', join(directory, 'a.ppm'), '--port', '0'],
  { stdio: ['ignore', 'pipe', 'pipe'] },
)
let logs = ''
service.stderr.on('data', (chunk) => {
  logs += chunk
})
const url = await new Promise((done, reject) => {
  const timer = setTimeout(() => reject(new Error(`Service startup timeout ${logs}`)), 15000)
  service.once('error', (error) => {
    clearTimeout(timer)
    reject(error)
  })
  service.stdout.on('data', (chunk) => {
    logs += chunk
    const match = logs.match(/http:\/\/127\.0\.0\.1:\d+/)
    if (match) {
      clearTimeout(timer)
      done(match[0])
    }
  })
  service.once('exit', (code) => {
    clearTimeout(timer)
    reject(new Error(`Service exited ${code}: ${logs}`))
  })
})
const browser = await chromium.launch({
  headless: true,
  args: ['--enable-webgl', '--use-angle=swiftshader', '--enable-unsafe-swiftshader'],
})
const report = []
try {
  const context = await browser.newContext({ viewport: { width: 1440, height: 900 } })
  const page = await context.newPage()
  const errors = [],
    requests = []
  page.on('pageerror', (error) => errors.push(error.message))
  page.on('request', (req) => {
    if (new URL(req.url()).pathname.startsWith('/api/image')) requests.push(new URL(req.url()))
  })
  await page.goto(url)
  const picker = page.getByRole('combobox', { name: '选择图片', exact: true })
  await picker.waitFor()
  await page.waitForFunction(() => !document.getElementById('image-select').disabled)
  assert.equal(await picker.inputValue(), 'a.ppm')
  assert.equal(await picker.locator('option').count(), 4)
  const listed = await (await context.request.get(`${url}/api/images`)).json()
  assert.ok(!listed.items.some((item) => item.name === 'ignore.txt'))
  async function load(name) {
    await picker.selectOption(name)
    await page.getByRole('button', { name: '后端加载并解码 BGRA8', exact: true }).click()
    await page.waitForFunction(
      () => document.getElementById('viewport').dataset.backendState === 'ready',
    )
    await page.getByRole('button', { name: '获取内存图片', exact: true }).click()
    await page.waitForFunction(
      (name) =>
        document.getElementById('viewport').dataset.name === name &&
        !document.getElementById('load-image').disabled,
      name,
    )
  }
  await load('a.ppm')
  assert.match(await page.locator('#image-info').textContent(), /192 × 128/)
  await load('蓝色 图片 #2.PPM')
  assert.match(await page.locator('#image-info').textContent(), /96 × 80/)
  assert.ok(
    requests.some(
      (req) => req.pathname === '/api/image' && req.searchParams.get('name') === '蓝色 图片 #2.PPM',
    ),
  )
  report.push('list, Unicode filenames and explicit per-request selection')

  await load('large.ppm')
  await page.getByRole('button', { name: '1:1', exact: true }).click()
  await page.waitForFunction(() => Number(document.getElementById('viewport').dataset.tiles) > 0)
  const tileRequests = requests.filter((req) => req.pathname.includes('/tile/'))
  assert.ok(tileRequests.length > 0)
  assert.ok(tileRequests.every((req) => req.searchParams.get('name') === 'large.ppm'))
  const mode = page.getByRole('combobox', { name: '显示模式', exact: true })
  await mode.selectOption('full')
  const start = requests.length
  await page.getByRole('button', { name: '获取内存图片', exact: true }).click()
  await page.waitForFunction(
    () =>
      document.getElementById('viewport').dataset.mode === 'full' &&
      !document.getElementById('load-image').disabled,
  )
  const fullRequests = requests.slice(start)
  assert.ok(fullRequests.some((req) => req.pathname === '/api/image'))
  assert.ok(
    !fullRequests.some(
      (req) => req.pathname.includes('/tile/') || req.pathname.includes('/preview'),
    ),
  )
  assert.equal(await page.locator('#viewport').getAttribute('data-tiles'), '0')
  await page.getByRole('button', { name: '1:1', exact: true }).click()
  await page.locator('#viewport').focus()
  await page.locator('#viewport').press('+')
  await page.waitForFunction(() => document.getElementById('zoom-value').textContent === '125.0%')
  await page.screenshot({ path: resolve(output, 'catalog-full-mode.png') })
  report.push('tile mode and full-image non-tiled mode stay separate')

  await page.route('**/api/image?*', async (route) => {
    if (new URL(route.request().url()).searchParams.get('name') === 'a.ppm') {
      const response = await route.fetch()
      await new Promise((done) => setTimeout(done, 350))
      await route.fulfill({ response })
    } else await route.continue()
  })
  await picker.selectOption('a.ppm')
  await page.getByRole('button', { name: '后端加载并解码 BGRA8', exact: true }).click()
  await page.waitForFunction(
    () => document.getElementById('viewport').dataset.backendState === 'ready',
  )
  await page.getByRole('button', { name: '获取内存图片', exact: true }).click()
  await picker.selectOption('蓝色 图片 #2.PPM')
  await page.getByRole('button', { name: '后端加载并解码 BGRA8', exact: true }).click()
  await page.waitForFunction(
    () => document.getElementById('viewport').dataset.backendState === 'ready',
  )
  await page.getByRole('button', { name: '获取内存图片', exact: true }).click()
  await page.waitForFunction(
    () =>
      document.getElementById('viewport').dataset.name === '蓝色 图片 #2.PPM' &&
      !document.getElementById('load-image').disabled,
  )
  await page.waitForTimeout(450)
  assert.equal(await page.locator('#viewport').getAttribute('data-name'), '蓝色 图片 #2.PPM')
  await page.unroute('**/api/image?*')
  report.push('late previous selection cannot replace the latest image')

  await picker.selectOption('bad.png')
  await page.getByRole('button', { name: '后端加载并解码 BGRA8', exact: true }).click()
  await page.waitForFunction(() =>
    document.getElementById('status').textContent.includes('解码失败'),
  )
  assert.equal(await page.locator('#viewport').getAttribute('data-name'), '蓝色 图片 #2.PPM')
  await ppm('bad.png', 25, 30, [70, 130, 160])
  await page.getByRole('button', { name: '后端加载并解码 BGRA8', exact: true }).click()
  await page.waitForFunction(
    () => document.getElementById('viewport').dataset.backendState === 'ready',
  )
  await page.getByRole('button', { name: '获取内存图片', exact: true }).click()
  await page.waitForFunction(
    () =>
      document.getElementById('viewport').dataset.name === 'bad.png' &&
      !document.getElementById('load-image').disabled,
  )
  report.push('corrupt image is reported and can be retried after replacement')

  await ppm('new.ppm', 20, 24, [71, 44, 91])
  await page.getByRole('button', { name: '刷新列表', exact: true }).click()
  await page.waitForFunction(() => document.querySelectorAll('#image-select option').length === 5)
  assert.equal(await picker.inputValue(), 'bad.png')
  await page.setViewportSize({ width: 390, height: 844 })
  await page.screenshot({ path: resolve(output, 'catalog-mobile.png') })
  assert.equal(await page.locator('body').evaluate((e) => e.scrollWidth <= innerWidth), true)
  report.push('list refresh and narrow-screen controls')

  const unsafe = await context.request.get(`${url}/api/image/info?name=..%2Foutside.png`)
  assert.equal(unsafe.status(), 400)
  assert.deepEqual(errors, [])
  await context.close()
  for (const name of report) console.log(`PASS ${name}`)
  console.log(`${report.length} catalog/mode browser scenarios passed.`)
} finally {
  await writeFile(resolve(output, 'catalog-results.json'), JSON.stringify({ report }, null, 2))
  await browser.close()
  const stopped = once(service, 'exit')
  service.kill('SIGTERM')
  await stopped
  await rm(directory, { recursive: true, force: true })
}
