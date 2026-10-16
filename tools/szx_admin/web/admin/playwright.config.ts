import { defineConfig } from '@playwright/test'
export default defineConfig({
  testDir: './tests/e2e',
  fullyParallel: true,
  use: {
    baseURL: 'http://localhost:5183',
    browserName: 'chromium',
    locale: 'zh-CN',
    channel: process.env.PLAYWRIGHT_CHANNEL || undefined,
    headless: true,
  },
  webServer: {
    command: 'npm run dev -- --port 5183 --strictPort',
    url: 'http://localhost:5183',
    reuseExistingServer: false,
  },
})
