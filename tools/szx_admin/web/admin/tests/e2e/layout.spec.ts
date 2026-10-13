import { chooseOption } from './select'
import { test, expect, type Locator, type Page } from '@playwright/test'

// Layout checks must exercise visible native scrollbars, including in headless Chromium.
test.use({ launchOptions: { ignoreDefaultArgs: ['--hide-scrollbars'] } })

// The app follows browser language; layout checks use the Chinese UI explicitly.
test.use({ locale: 'zh-CN' })

async function signInWithRows(page: Page) {
  const user = {
    id: 'layout-admin',
    login_identifier: 'admin',
    display_name: '系统管理员',
    role: 'super_admin',
    status: 'active',
    version: 1,
    authz_version: 1,
    force_password_change: false,
    password_change_recommended: false,
    created_at: Date.now(),
    updated_at: Date.now(),
    last_login_at: null,
  }
  await page.route('**/api/v1/**', async (route) => {
    const path = new URL(route.request().url()).pathname
    let data: unknown
    if (path.endsWith('/auth/login')) {
      data = {
        user,
        access_token: 'layout-access',
        csrf_token: 'layout-csrf',
        expires_at: Date.now() + 900_000,
        expires_in: 900,
        authz_version: 1,
        token_type: 'Bearer',
      }
    } else if (path.endsWith('/client/me/permissions')) {
      data = {
        user_id: user.id,
        role: user.role,
        capabilities: ['user.read', 'audit.read'],
        authz_version: 1,
        status: 'active',
        force_password_change: false,
      }
    } else {
      const items = Array.from({ length: 20 }, (_, index) => {
        if (path.endsWith('/admin/users')) {
          return {
            ...user,
            id: `layout-user-${index}`,
            login_identifier: `operator-${index}`,
            display_name: `操作员 ${index}`,
            role: 'operator',
          }
        }
        return {
          id: `layout-event-${index}`,
          event_name: path.endsWith('/behavior-events') ? 'report.view' : 'auth.login',
          result: 'success',
          actor_user_id: user.id,
          actor_login_identifier: user.login_identifier,
          user_id: user.id,
          login_identifier: user.login_identifier,
          target_user_id: user.id,
          request_id: `layout-request-${index}`,
          details: { source: 'web' },
          created_at: Date.now(),
        }
      })
      data = { items, total: 40, page: 1, page_size: 20 }
    }
    await route.fulfill({ json: { data, request_id: 'layout-request' } })
  })
  await page.goto('/login')
  await page.locator('input[name="identifier"]').fill('admin')
  await page.locator('input[name="password"]').fill('TestPassword123')
  await page.locator('.login-submit').click()
  await expect(page).toHaveURL('/users')
}

async function expectOnlyTableToScroll(page: Page) {
  await expect(page.locator('.page-heading .eyebrow')).toHaveCount(0)
  const topGap = await page.locator('.page-heading').evaluate((element) => {
    return (
      element.getBoundingClientRect().top -
      document.querySelector('.topbar')!.getBoundingClientRect().bottom
    )
  })
  expect(topGap).toBeLessThan(24)
  const table = page.locator('.table-scroll')
  await expect(table.locator('tbody tr')).toHaveCount(20)
  await page.evaluate(() => document.fonts.ready)
  const fixed = page.locator('.topbar, .page-heading, .table-heading, .pagination, .app-footer')
  const positions = await fixed.evaluateAll((elements) =>
    elements.map((element) => element.getBoundingClientRect().top),
  )
  const height = await table.evaluate((element) => element.clientHeight)
  expect(height).toBeGreaterThan(70)
  await table.hover()
  await page.mouse.wheel(0, 800)
  await expect.poll(() => table.evaluate((element) => element.scrollTop)).toBeGreaterThan(0)
  expect(
    await fixed.evaluateAll((elements) =>
      elements.map((element) => element.getBoundingClientRect().top),
    ),
  ).toEqual(positions)
  expect(
    await page.evaluate(() => ({
      pageScroll: document.scrollingElement?.scrollTop,
      mainScroll: document.querySelector('.main')?.scrollTop,
      workspaceScroll: document.querySelector('.workspace')?.scrollTop,
    })),
  ).toMatchObject({ pageScroll: 0, mainScroll: 0, workspaceScroll: 0 })
  const bottomGap = await page.locator('.table-panel').evaluate((element) => {
    const footer = document.querySelector('.app-footer')!
    return footer.getBoundingClientRect().top - element.getBoundingClientRect().bottom
  })
  expect(bottomGap).toBeLessThan(60)
  await expect(page.locator('.pagination')).toBeInViewport()
  await expect(page.locator('.app-footer')).toBeInViewport()
  const dimensions = await page.evaluate(() => ({
    width: document.documentElement.scrollWidth,
    height: document.documentElement.scrollHeight,
    viewportWidth: innerWidth,
    viewportHeight: innerHeight,
  }))
  expect(dimensions.width).toBeLessThanOrEqual(dimensions.viewportWidth)
  expect(dimensions.height).toBeLessThanOrEqual(dimensions.viewportHeight)
}

for (const viewport of [
  { width: 2232, height: 1281 },
  { width: 1440, height: 900 },
  { width: 1366, height: 768 },
  { width: 390, height: 844 },
]) {
  test(`data tables scroll inside a fixed workspace at ${viewport.width}×${viewport.height}`, async ({
    page,
  }) => {
    await page.setViewportSize(viewport)
    await signInWithRows(page)
    await expectOnlyTableToScroll(page)
    if (viewport.width === 2232) {
      await page.screenshot({ path: 'test-results/users-layout-desktop.png' })
    }
    await page.locator('.sidebar a[href="/logs"]').click()
    await expect(page).toHaveURL('/logs')
    await expect(page.locator('.logs-page')).toBeVisible()
    await expectOnlyTableToScroll(page)
    await page.getByRole('tab', { name: '用户行为' }).click()
    await expect(page.locator('.table-scroll tbody tr').first()).toContainText('report.view')
    await expectOnlyTableToScroll(page)
  })
}

test('language focus surrounds the complete component and effects start disabled', async ({
  page,
}) => {
  await page.goto('/login')
  const select = page.locator('.language-switcher').getByRole('combobox')
  await select.focus()
  expect(
    await select.evaluate((element) => {
      const control = getComputedStyle(element)
      const container = getComputedStyle(element.closest('.language-switcher')!)
      return {
        outline: control.outlineStyle,
        innerShadow: control.boxShadow,
        shadow: container.boxShadow,
      }
    }),
  ).toMatchObject({
    outline: 'none',
    innerShadow: 'none',
    shadow: expect.not.stringMatching(/^none$/),
  })
  await select.click()
  await expect(page.getByRole('option')).toHaveCount(6)
  await expect(page.getByRole('option', { name: 'English', exact: true })).toHaveCount(1)
  await page.getByRole('option', { name: 'English', exact: true }).click()
  await expect(page.locator('.login-submit')).toContainText('Sign in')
  await expect(page.locator('html')).toHaveAttribute('data-fx', 'lite')
  expect(await page.evaluate(() => document.getAnimations().length)).toBe(0)
})

for (const savedLocale of ['en-US', 'en-GB']) {
  test(`saved ${savedLocale} preference uses the single English option`, async ({ page }) => {
    await page.addInitScript((locale) => {
      localStorage.setItem('szx-admin-locale', locale)
    }, savedLocale)
    await page.goto('/login')
    await expect(page.locator('.language-switcher').getByRole('combobox')).toHaveText('English')
    await expect(page.locator('.login-submit')).toContainText('Sign in')
    await expect(page.locator('html')).toHaveAttribute('lang', 'en-US')
  })
}

test.describe('English browser language', () => {
  test.use({ locale: 'en-GB' })

  test('UK browser language resolves to the single English option', async ({ page }) => {
    await page.goto('/login')
    await expect(page.locator('.language-switcher').getByRole('combobox')).toHaveText('English')
    await expect(page.locator('.login-submit')).toContainText('Sign in')
    await expect(page.locator('html')).toHaveAttribute('lang', 'en-US')

    await chooseOption(page, page.locator('.language-switcher').getByRole('combobox'), '简体中文')
    await chooseOption(page, page.locator('.language-switcher').getByRole('combobox'), 'English')
    expect(await page.evaluate(() => localStorage.getItem('szx-admin-locale'))).toBe('en-US')
    await page.reload()
    await expect(page.locator('.language-switcher').getByRole('combobox')).toHaveText('English')
    await expect(page.locator('.login-submit')).toContainText('Sign in')
  })
})

test('Aura follows Lumina dark mode in teleported dropdowns and dialogs', async ({ page }) => {
  await signInWithRows(page)
  await page.evaluate(() => {
    document.documentElement.dataset.theme = 'dark'
  })
  await page.locator('.language-switcher').getByRole('combobox').click()
  const overlay = page.locator('.p-select-overlay')
  await expect(overlay).toBeVisible()
  const palette = await overlay.evaluate((element) => ({
    background: getComputedStyle(element).backgroundColor,
    color: getComputedStyle(element).color,
  }))
  expect(palette).toEqual({ background: 'rgb(20, 24, 51)', color: 'rgb(233, 236, 255)' })
  await page.keyboard.press('Escape')
  await page.getByRole('button', { name: '创建用户' }).click()
  const dialog = page.getByRole('dialog')
  await expect(dialog).toBeVisible()
  expect(
    await dialog.evaluate((element) => ({
      background: getComputedStyle(element).backgroundColor,
      color: getComputedStyle(element).color,
    })),
  ).toEqual({ background: 'rgb(20, 24, 51)', color: palette.color })
  await dialog.getByRole('combobox', { name: '角色', exact: true }).click()
  await expect(page.getByRole('option', { name: '工程师', exact: true })).toBeVisible()
  expect(
    await page
      .locator('.p-select-overlay')
      .evaluate((element) => getComputedStyle(element).backgroundColor),
  ).toBe(palette.background)
  await page.screenshot({ path: 'test-results/users-aura-dark-dialog.png' })
})

test('PrimeVue pagination requests server pages and disables navigation while loading', async ({
  page,
}) => {
  await signInWithRows(page)
  let release!: () => void
  const pending = new Promise<void>((resolve) => {
    release = resolve
  })
  await page.route('**/api/v1/admin/users?*', async (route) => {
    const url = new URL(route.request().url())
    const target = Number(url.searchParams.get('page'))
    if (target === 2) await pending
    await route.fulfill({
      json: {
        data: { items: [], total: 40, page: target, page_size: 20 },
        request_id: 'pagination-request',
      },
    })
  })
  const next = page.getByRole('button', { name: '下一页', exact: true })
  const previous = page.getByRole('button', { name: '上一页', exact: true })
  const request = page.waitForRequest(
    (request) => new URL(request.url()).searchParams.get('page') === '2',
  )
  await next.click()
  expect(new URL((await request).url()).searchParams.get('page_size')).toBe('20')
  try {
    await expect(next).toBeDisabled()
    await expect(previous).toBeDisabled()
  } finally {
    release()
  }
  await expect(page.locator('.pagination')).toContainText('2 / 2')
  await expect(next).toBeDisabled()
  await expect(previous).toBeEnabled()
  await previous.click()
  await expect(page.locator('.pagination')).toContainText('1 / 2')
  await chooseOption(page, page.locator('.language-switcher').getByRole('combobox'), 'English')
  await expect(page.getByRole('button', { name: 'Previous', exact: true })).toBeDisabled()
  await expect(page.getByRole('button', { name: 'Next', exact: true })).toBeEnabled()
})

test.describe('dropdown scrollbar clipping', () => {
  for (const theme of ['light', 'dark']) {
    for (const dropdown of ['language', 'role']) {
      test(`rounded dropdown clips its scrollbar: ${dropdown}, ${theme}`, async ({ page }) => {
        if (dropdown === 'role') await signInWithRows(page)
        else await page.goto('/login')
        await page.evaluate((theme) => {
          document.documentElement.dataset.theme = theme
        }, theme)
        // Force overflow and classic, opaque scrollbars even on overlay-scrollbar hosts.
        await page.addStyleTag({
          content: `
        .p-select-list-container { max-height: 96px !important; scrollbar-gutter: stable; }
        .p-select-list-container::-webkit-scrollbar { width: 16px; }
        .p-select-list-container::-webkit-scrollbar-track { background: #f59e0b; }
        .p-select-list-container::-webkit-scrollbar-thumb { background: #737a9c; }
      `,
        })
        const control =
          dropdown === 'language'
            ? page.locator('.language-switcher').getByRole('combobox')
            : page.getByRole('combobox', { name: '角色', exact: true })
        await control.click()
        const overlay = page.locator('.p-select-overlay')
        const scroller = overlay.locator('.p-select-list-container')
        await expect(overlay).toBeVisible()
        const clipping = await overlay.evaluate((element) => {
          const style = getComputedStyle(element)
          const scroller = element.querySelector('.p-select-list-container')!
          return {
            overflowX: style.overflowX,
            overflowY: style.overflowY,
            radius: style.borderTopRightRadius,
            scrollRadius: getComputedStyle(scroller).borderTopRightRadius,
          }
        })
        expect(clipping.overflowX).toBe('hidden')
        expect(clipping.overflowY).toBe('hidden')
        expect(parseFloat(clipping.radius)).toBeGreaterThan(0)
        expect(clipping.scrollRadius).toBe(clipping.radius)
        const screenshot = await page.screenshot()
        const paint = await page.evaluate(async (png) => {
          const image = new Image()
          image.src = `data:image/png;base64,${png}`
          await image.decode()
          const canvas = document.createElement('canvas')
          canvas.width = image.naturalWidth
          canvas.height = image.naturalHeight
          const context = canvas.getContext('2d')!
          context.drawImage(image, 0, 0)
          const rect = document.querySelector('.p-select-overlay')!.getBoundingClientRect()
          const scale = image.naturalWidth / innerWidth
          const pixel = (y: number) => {
            const [r, g, b] = context.getImageData(
              Math.floor((rect.right - 2) * scale),
              Math.floor(y * scale),
              1,
              1,
            ).data
            return `${r},${g},${b}`
          }
          return {
            corners: [pixel(rect.top + 2), pixel(rect.bottom - 2)],
            track: pixel(rect.top + rect.height / 2),
          }
        }, screenshot.toString('base64'))
        const scrollbarColors = ['245,158,11', '115,122,156']
        expect(scrollbarColors).toContain(paint.track)
        for (const corner of paint.corners) expect(scrollbarColors).not.toContain(corner)
        expect(
          await scroller.evaluate((element) => element.scrollHeight > element.clientHeight),
        ).toBe(true)
        expect(
          await scroller.evaluate(
            (element) => element.getBoundingClientRect().width - element.clientWidth,
          ),
        ).toBeGreaterThan(0)
        await scroller.hover()
        await page.mouse.wheel(0, 200)
        await expect
          .poll(() => scroller.evaluate((element) => element.scrollTop))
          .toBeGreaterThan(0)
        await page.screenshot({ path: `test-results/dropdown-scrollbar-${dropdown}-${theme}.png` })
        await control.focus()
        await page.keyboard.press('End')
        await page.keyboard.press('Enter')
        await expect(overlay).toHaveCount(0)
        await expect(control).toHaveText(dropdown === 'language' ? 'Русский' : '超级管理员')
      })
    }
  }
})

for (const theme of ['light', 'dark']) {
  test(`select keyboard focus surrounds the complete rounded control: ${theme}`, async ({
    page,
  }) => {
    await signInWithRows(page)
    await page.evaluate((theme) => {
      document.documentElement.dataset.theme = theme
    }, theme)
    await page.keyboard.press('Tab')
    for (const label of ['角色', '状态', '排序']) {
      const control = page.getByRole('combobox', { name: label, exact: true })
      await control.focus()
      await expect(page.locator('.p-select.p-focus')).toHaveCount(1)
      const focus = await control.evaluate((element) => {
        const inner = getComputedStyle(element)
        const root = element.closest('.p-select')!
        const outer = getComputedStyle(root)
        return {
          visible: element.matches(':focus-visible'),
          innerOutline: inner.outlineStyle,
          innerShadow: inner.boxShadow,
          outerShadow: outer.boxShadow,
          outerRadius: outer.borderRadius,
          coversArrow: root.getBoundingClientRect().width > element.getBoundingClientRect().width,
        }
      })
      expect(focus.visible).toBe(true)
      expect(focus.innerOutline).toBe('none')
      expect(focus.innerShadow).toBe('none')
      expect(focus.outerShadow).not.toBe('none')
      expect(parseFloat(focus.outerRadius)).toBeGreaterThan(0)
      expect(focus.coversArrow).toBe(true)
    }
    await page.screenshot({ path: `test-results/select-rounded-focus-${theme}.png` })
    const button = page.getByRole('button', { name: '创建用户' })
    await button.focus()
    expect(await button.evaluate((element) => getComputedStyle(element).outlineStyle)).toBe('solid')
  })
}

for (const viewport of [
  { width: 1440, height: 900 },
  { width: 2232, height: 1281 },
]) {
  test(`select text and arrow stay vertically centered at ${viewport.width}×${viewport.height}`, async ({
    page,
  }) => {
    await page.setViewportSize(viewport)
    await signInWithRows(page)
    await page.evaluate(() => document.fonts.ready)
    const expectCentered = async (control: Locator) => {
      const offsets = await control.evaluate((element) => {
        const range = document.createRange()
        range.selectNodeContents(element)
        const text = range.getBoundingClientRect()
        const root = element.closest('.p-select')!
        const rect = root.getBoundingClientRect()
        const arrow = root.querySelector('.p-select-dropdown')!.getBoundingClientRect()
        const center = rect.top + rect.height / 2
        return {
          text: text.top + text.height / 2 - center,
          arrow: arrow.top + arrow.height / 2 - center,
        }
      })
      expect(Math.abs(offsets.text)).toBeLessThanOrEqual(2)
      expect(Math.abs(offsets.arrow)).toBeLessThanOrEqual(1)
    }
    for (const control of await page.locator('.p-select-label').all()) await expectCentered(control)
    await page.getByRole('link', { name: '审计日志' }).click()
    await expect(page.locator('.logs-page')).toBeVisible()
    const result = page.getByRole('combobox', { name: '结果', exact: true })
    await chooseOption(page, result, '失败')
    for (const control of await page.locator('.p-select-label').all()) await expectCentered(control)
    await page.screenshot({ path: `test-results/select-centered-${viewport.width}.png` })
  })
}

for (const theme of ['light', 'dark']) {
  test(`date/time popup uses readable Lumina styling: ${theme}`, async ({ page }) => {
    await signInWithRows(page)
    await page.getByRole('link', { name: '审计日志' }).click()
    await page.evaluate((theme) => {
      document.documentElement.dataset.theme = theme
    }, theme)
    const input = page.getByRole('combobox', { name: '结束时间（本地）', exact: true })
    await input.fill('2026-10-13 14:43:01')
    await input.press('ArrowDown')
    const popup = page.locator('.lumina-datetime-panel')
    await expect(popup).toBeVisible()
    const appearance = await popup.evaluate((element) => {
      const style = getComputedStyle(element)
      return {
        fontSize: parseFloat(style.fontSize),
        radius: parseFloat(style.borderRadius),
        border: style.borderTopStyle,
        borderWidth: parseFloat(style.borderTopWidth),
        shadow: style.boxShadow,
        timeFontSize: parseFloat(
          getComputedStyle(element.querySelector('.p-datepicker-time-picker')!).fontSize,
        ),
      }
    })
    expect(appearance.fontSize).toBeGreaterThanOrEqual(13.8)
    expect(appearance.timeFontSize).toBeGreaterThanOrEqual(14.7)
    expect(appearance.radius).toBeGreaterThan(10)
    expect(appearance.border).toBe('solid')
    expect(appearance.borderWidth).toBeGreaterThan(0)
    expect(appearance.shadow).not.toBe('none')
    await expect(popup.getByRole('button', { name: '今天', exact: true })).toBeVisible()
    await expect(popup.getByRole('button', { name: '清除', exact: true })).toBeVisible()
    await popup.getByRole('button', { name: '下一秒', exact: true }).click()
    await expect(input).toHaveValue('2026-10-13 14:43:02')
    await page.screenshot({ path: `test-results/datetime-popup-${theme}.png` })
    await popup.getByRole('button', { name: '清除', exact: true }).click()
    await expect(input).toHaveValue('')
    await expect(popup).toHaveCount(0)
  })
  test(`dialog close button is a subdued wide rectangle: ${theme}`, async ({ page }) => {
    await signInWithRows(page)
    await page.evaluate((theme) => {
      document.documentElement.dataset.theme = theme
    }, theme)
    await page.getByRole('button', { name: '创建用户' }).click()
    const close = page.getByRole('dialog').getByRole('button', { name: '关闭', exact: true })
    const shape = await close.evaluate((element) => {
      const rect = element.getBoundingClientRect()
      const style = getComputedStyle(element)
      return {
        width: rect.width,
        height: rect.height,
        radius: parseFloat(style.borderRadius),
        background: style.backgroundColor,
      }
    })
    expect(shape.width).toBeGreaterThan(shape.height)
    expect(shape.radius).toBeGreaterThan(0)
    expect(shape.radius).toBeLessThan(shape.height / 2)
    expect(shape.background).not.toBe('rgb(91, 108, 255)')
    await close.focus()
    expect(await close.evaluate((element) => getComputedStyle(element).boxShadow)).not.toBe('none')
    await page.screenshot({ path: `test-results/dialog-close-wide-${theme}.png` })
    await close.click()
    await expect(page.getByRole('dialog')).toHaveCount(0)
  })
}

test('date/time popup fits mobile viewport and follows language changes', async ({ page }) => {
  await page.setViewportSize({ width: 390, height: 844 })
  await signInWithRows(page)
  await page.getByRole('link', { name: '审计日志' }).click()
  const input = page.getByRole('combobox', { name: '结束时间（本地）', exact: true })
  await input.scrollIntoViewIfNeeded()
  await input.press('ArrowDown')
  const popup = page.locator('.lumina-datetime-panel')
  await expect(popup).toBeVisible()
  const bounds = await popup.boundingBox()
  expect(bounds!.x).toBeGreaterThanOrEqual(0)
  expect(bounds!.x + bounds!.width).toBeLessThanOrEqual(390)
  expect(bounds!.y).toBeGreaterThanOrEqual(0)
  expect(bounds!.y + bounds!.height).toBeLessThanOrEqual(844)
  await popup.getByRole('button', { name: '清除', exact: true }).click()
  await chooseOption(page, page.locator('.language-switcher').getByRole('combobox'), 'English')
  const english = page.getByRole('combobox', { name: 'End time (local)', exact: true })
  await english.scrollIntoViewIfNeeded()
  await english.press('ArrowDown')
  await expect(popup.getByRole('button', { name: 'Today', exact: true })).toBeVisible()
  await expect(popup.getByRole('button', { name: 'Clear', exact: true })).toBeVisible()
  await page.screenshot({ path: 'test-results/datetime-popup-mobile.png' })
})

for (const theme of ['light', 'dark']) {
  test(`business table retains spacing and dialog has an opaque surface: ${theme}`, async ({
    page,
  }) => {
    await signInWithRows(page)
    await page.evaluate((theme) => {
      document.documentElement.dataset.theme = theme
    }, theme)
    const table = page.locator('.admin-table')
    const header = await table
      .locator('th')
      .first()
      .evaluate((element) => {
        const style = getComputedStyle(element)
        return {
          paddingX: parseFloat(style.paddingLeft),
          paddingY: parseFloat(style.paddingTop),
          background: style.backgroundColor,
        }
      })
    const cell = await table
      .locator('tbody td')
      .first()
      .evaluate((element) => {
        const style = getComputedStyle(element)
        return {
          paddingX: parseFloat(style.paddingLeft),
          paddingY: parseFloat(style.paddingTop),
          border: style.borderBottomColor,
        }
      })
    expect(header.paddingX).toBeGreaterThan(10)
    expect(header.paddingY).toBeGreaterThan(8)
    expect(header.background).toBe(theme === 'light' ? 'rgb(244, 246, 253)' : 'rgb(38, 43, 69)')
    expect(cell.paddingX).toBeGreaterThan(10)
    expect(cell.paddingY).toBeGreaterThan(10)
    expect(cell.border).toBe(
      theme === 'light' ? 'rgba(20, 24, 51, 0.1)' : 'rgba(255, 255, 255, 0.1)',
    )
    await page.getByRole('button', { name: '创建用户' }).click()
    const dialog = page.getByRole('dialog')
    const surface = await dialog.evaluate((element) => {
      const style = getComputedStyle(element)
      return {
        background: style.backgroundColor,
        radius: parseFloat(style.borderRadius),
        shadow: style.boxShadow,
      }
    })
    expect(surface.background).toBe(theme === 'light' ? 'rgb(255, 255, 255)' : 'rgb(20, 24, 51)')
    expect(surface.radius).toBeGreaterThan(16)
    expect(surface.shadow).not.toBe('none')
    await page.screenshot({ path: `test-results/table-and-dialog-restored-${theme}.png` })
  })
}

test('primary actions keep the brand gradient across login, users, dialogs and logs', async ({
  page,
}) => {
  await page.goto('/login')
  const gradient = async (control: Locator) => {
    const style = await control.evaluate((element) => getComputedStyle(element).backgroundImage)
    expect(style).toContain('linear-gradient')
    expect(style).toContain('rgb(91, 108, 255)')
    expect(style).toContain('rgb(168, 85, 247)')
  }
  await gradient(page.locator('.login-submit'))
  await signInWithRows(page)
  await gradient(page.getByRole('button', { name: '创建用户' }))
  await gradient(page.getByRole('button', { name: '查询', exact: true }))
  await page.getByRole('button', { name: '创建用户' }).click()
  await gradient(page.getByRole('dialog').getByRole('button', { name: '确认', exact: true }))
  await page.getByRole('dialog').getByRole('button', { name: '取消', exact: true }).click()
  await page.getByRole('link', { name: '审计日志' }).click()
  await gradient(page.getByRole('button', { name: '查询日志', exact: true }))
  await page.getByRole('tab', { name: '用户行为' }).click()
  await gradient(page.getByRole('button', { name: '查询事件', exact: true }))
})

test('shared popup surfaces remain opaque when runtime component tokens are unavailable', async ({
  page,
}) => {
  await signInWithRows(page)
  await page.getByRole('link', { name: '审计日志' }).click()
  await page.getByRole('combobox', { name: '开始时间（本地）', exact: true }).press('ArrowDown')
  await page.evaluate(() => {
    document
      .querySelectorAll(
        'style[data-primevue-style-id="datepicker-variables"], style[data-primevue-style-id="datatable-variables"]',
      )
      .forEach((element) => element.remove())
  })
  const popup = page.locator('.lumina-datetime-panel')
  expect(await popup.evaluate((element) => getComputedStyle(element).backgroundColor)).toBe(
    'rgb(255, 255, 255)',
  )
  expect(
    await page
      .locator('.admin-table th')
      .first()
      .evaluate((element) => parseFloat(getComputedStyle(element).paddingLeft)),
  ).toBeGreaterThan(10)
  await popup.getByRole('button', { name: '清除', exact: true }).click()
  await page.locator('.language-switcher').getByRole('combobox').click()
  await page.evaluate(() => {
    document.querySelector('style[data-primevue-style-id="select-variables"]')?.remove()
  })
  expect(
    await page
      .locator('.p-select-overlay')
      .evaluate((element) => getComputedStyle(element).backgroundColor),
  ).toBe('rgb(255, 255, 255)')
  await page.keyboard.press('Escape')
  await page.getByRole('link', { name: '用户管理' }).click()
  await page.getByRole('button', { name: '创建用户' }).click()
  await page.evaluate(() => {
    document.querySelector('style[data-primevue-style-id="dialog-variables"]')?.remove()
  })
  expect(
    await page.getByRole('dialog').evaluate((element) => getComputedStyle(element).backgroundColor),
  ).toBe('rgb(255, 255, 255)')
})
