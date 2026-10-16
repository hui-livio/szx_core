import { chromium } from '@playwright/test'
import { readFile, mkdir, writeFile } from 'node:fs/promises'
import { resolve } from 'node:path'
import { once } from 'node:events'

// Run against the Vite dev server or preview with the fixture available.
const args = process.argv.slice(2)
const option = (name, fallback) => {
  const index = args.indexOf(name)
  return index < 0 ? fallback : args[index + 1]
}
const baseURL = option('--url', 'http://localhost:5183')
const reference = option('--reference', undefined)
const duration = Number(option('--duration', '3000'))
if (!Number.isFinite(duration) || duration < 1000) throw new Error('--duration must be >= 1000 ms')
const output = resolve(option('--output', 'test-results/background-profile'))
await mkdir(output, { recursive: true })
const browser = await chromium.launch({ headless: true })
const report = {
  note: 'Headless Chromium rendering proxies, NOT hardware GPU utilization or VRAM measurements. Layer area is an estimate, not allocated memory.',
  browser: browser.version(),
  platform: process.platform,
  durationMs: duration,
  samples: [],
}

async function referenceMarkup() {
  const root = resolve(reference, 'src/ui/lumina/theme')
  const theme = await readFile(resolve(root, 'glass/theme.css'), 'utf8')
  const tokens = await readFile(resolve(root, 'tokens.css'), 'utf8')
  const sparks = Array.from({ length: 14 }, (_, i) => {
    const style = `left:${((i * 61.8034) % 100).toFixed(2)}%;--lg-spark-size:${3 + (i % 3)}px;animation-duration:${17 + (i % 7) * 2.4}s;animation-delay:-${((i * 2.63) % 19).toFixed(2)}s`
    return `<div class="lg-spark" style="${style}"></div>`
  }).join('')
  return `<!doctype html><html data-theme="light" data-fx="full"><head><style>${theme}\n${tokens}</style></head><body><div class="lg-bg"><div class="lg-orb a"></div><div class="lg-orb b"></div><div class="lg-orb c"></div><div class="lg-spot"></div><div class="lg-grid"></div><div class="lg-noise"></div>${sparks}</div></body></html>`
}

try {
  const browserSession = await browser.newBrowserCDPSession()
  const system = await browserSession.send('SystemInfo.getInfo')
  report.gpu = {
    devices: system.gpu.devices,
    renderer: system.gpu.auxAttributes?.glRenderer ?? null,
    vendor: system.gpu.auxAttributes?.glVendor ?? null,
  }
  await browserSession.detach()
  for (const viewport of [
    { width: 1920, height: 1080 },
    { width: 3840, height: 2160 },
  ]) {
    for (const mode of [
      ...(reference ? ['reference-full'] : []),
      'optimized-full',
      'optimized-lite',
    ]) {
      const dpr = 2
      const context = await browser.newContext({
        viewport,
        deviceScaleFactor: dpr,
        reducedMotion: 'no-preference',
      })
      const page = await context.newPage()
      if (mode === 'reference-full') await page.setContent(await referenceMarkup())
      else {
        await page.goto(`${baseURL}/tests/fixtures/lumina/background.html`)
        await page.locator('.lg-bg').waitFor()
        if (mode === 'optimized-full')
          await page.getByRole('button', { name: 'Toggle effects' }).click()
        // Exclude fixture controls from screenshots/layer accounting.
        await page.locator('main').evaluate((element) => {
          element.style.display = 'none'
        })
      }
      await page.evaluate(() => {
        document.documentElement.style.setProperty('--k', '1')
      })
      const session = await context.newCDPSession(page)
      let layers
      session.on('LayerTree.layerTreeDidChange', (event) => {
        if (event.layers) layers = event.layers
      })
      await session.send('LayerTree.enable')
      await session.send('Performance.enable')
      await page.screenshot()
      await page.waitForTimeout(1500)
      const events = []
      session.on('Tracing.dataCollected', ({ value }) => events.push(...value))
      await session.send('Tracing.start', {
        categories: 'devtools.timeline,disabled-by-default-devtools.timeline,cc',
        transferMode: 'ReportEvents',
      })
      const before = await session.send('Performance.getMetrics')
      await page.waitForTimeout(duration)
      const after = await session.send('Performance.getMetrics')
      const completed = once(session, 'Tracing.tracingComplete')
      await session.send('Tracing.end')
      await completed
      const metric = (data, name) => data.metrics.find((m) => m.name === name)?.value ?? 0
      const milliseconds = (name) =>
        Number(((metric(after, name) - metric(before, name)) * 1000).toFixed(3))
      const count = (name) =>
        events.filter((event) => event.name === name && event.ph === 'X').length
      const sprites = await page.locator('.lg-orb, .lg-spark, .lg-spot').evaluateAll((elements) =>
        elements
          .filter((element) => getComputedStyle(element).display !== 'none')
          .map((element) => ({
            width: element.getBoundingClientRect().width,
            height: element.getBoundingClientRect().height,
          })),
      )
      const sample = {
        mode,
        viewport,
        dpr,
        taskMs: milliseconds('TaskDuration'),
        scriptMs: milliseconds('ScriptDuration'),
        layoutMs: milliseconds('LayoutDuration'),
        styleMs: milliseconds('RecalcStyleDuration'),
        paintEvents: count('Paint'),
        rasterTasks: count('RasterTask'),
        contentLayers: layers?.filter((layer) => layer.drawsContent).length ?? null,
        estimatedContentLayerPixels: layers
          ? Math.round(
              layers
                .filter((layer) => layer.drawsContent)
                .reduce((sum, layer) => sum + layer.width * layer.height * dpr * dpr, 0),
            )
          : null,
        spriteCount: sprites.length,
        spriteAreaPixels: Math.round(
          sprites.reduce((sum, sprite) => sum + sprite.width * sprite.height * dpr * dpr, 0),
        ),
      }
      report.samples.push(sample)
      const name = `${viewport.width}-${mode}`
      await writeFile(
        resolve(output, `${name}.trace.json`),
        JSON.stringify({ traceEvents: events }),
      )
      await page.screenshot({ path: resolve(output, `${name}.png`) })
      console.log(JSON.stringify(sample))
      await context.close()
    }
  }
} finally {
  await browser.close()
  await mkdir(output, { recursive: true })
  await writeFile(resolve(output, 'summary.json'), JSON.stringify(report, null, 2))
}
console.log(`Report and traces: ${output}`)
