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
        capabilities: [
          'user.read',
          'audit.read',
          'behavior.read',
          'user.create',
          'user.update',
          'user.status',
          'user.unlock',
          'user.password.reset',
        ],
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
  await expect(page).toHaveURL('/home')
  await page.locator('.sidebar a[href="/users"]').click()
  await expect(page).toHaveURL('/users')
}

async function expectOnlyTableToScroll(page: Page) {
  await expect(page.locator('.page-heading .eyebrow')).toHaveCount(0)
  const topGap = await page.locator('.page-heading').evaluate((element) => {
    return (
      element.getBoundingClientRect().top -
      document.querySelector(innerWidth <= 720 ? '.sidebar' : '.topbar')!.getBoundingClientRect()
        .bottom
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

for (const width of [320, 390, 701, 720]) {
  test(`narrow header contains account actions and an icon language menu at ${width}px`, async ({
    page,
  }) => {
    const errors: string[] = []
    page.on('pageerror', (error) => errors.push(error.message))
    await page.setViewportSize({ width, height: 844 })
    await signInWithRows(page)
    const header = page.locator('.sidebar-header')
    const topbar = header.locator('.topbar')
    await expect(topbar).toBeVisible()
    await expect(page.locator('.workspace > .topbar')).toHaveCount(0)
    await expect(header.locator('.avatar')).toBeVisible()
    await expect(header.getByRole('link', { name: '修改密码', exact: true })).toHaveCount(0)
    await expect(header.getByRole('button', { name: '退出', exact: true })).toHaveCount(0)
    await expect(header.getByRole('button', { name: '账号菜单', exact: true })).toBeVisible()
    const bounds = await header.evaluate((element) => {
      const brand = element.querySelector('.brand')!.getBoundingClientRect()
      const actions = element.querySelector('.topbar')!.getBoundingClientRect()
      const navigation = document.querySelector('.sidebar nav')!.getBoundingClientRect()
      return { brand: brand.toJSON(), actions: actions.toJSON(), navigation: navigation.toJSON() }
    })
    expect(bounds.actions.bottom).toBeLessThanOrEqual(bounds.navigation.top)
    if (width >= 701) {
      expect(bounds.actions.left).toBeGreaterThan(bounds.brand.right)
      expect(
        Math.abs(
          (bounds.actions.top + bounds.actions.bottom) / 2 -
            (bounds.brand.top + bounds.brand.bottom) / 2,
        ),
      ).toBeLessThan(2)
    }
    if (width === 701) {
      await page.screenshot({ path: 'test-results/narrow-header.png' })
    }
    const language = header.locator('.language-switcher').getByRole('combobox')
    await expect(language.locator('.language-button-icon')).toBeVisible()
    await expect(language.locator('.language-value')).toBeHidden()
    await expect(header.locator('.language-icon')).toBeHidden()
    const size = await header.locator('.language-switcher .p-select').boundingBox()
    expect(Math.abs(size!.width - size!.height)).toBeLessThanOrEqual(1)
    await language.click()
    await expect(page.getByRole('option')).toHaveCount(6)
    const menu = await page.locator('.p-select-overlay').boundingBox()
    expect(menu!.x).toBeGreaterThanOrEqual(0)
    expect(menu!.x + menu!.width).toBeLessThanOrEqual(width)
    await page.getByRole('option', { name: 'English', exact: true }).click()
    await expect(page.locator('html')).toHaveAttribute('lang', 'en-US')
    await expect(page.getByRole('option')).toHaveCount(0)
    await language.focus()
    await language.press('ArrowDown')
    await expect(page.getByRole('option')).toHaveCount(6)
    await language.press('Escape')
    await expect(page.getByRole('option')).toHaveCount(0)
    await language.click()
    await header.locator('.brand').click()
    await expect(page.getByRole('option')).toHaveCount(0)
    expect(await page.evaluate(() => document.documentElement.scrollWidth)).toBeLessThanOrEqual(
      width,
    )
    // Reparent the same controls on resize without losing the selected language or preferences.
    await page.setViewportSize({ width: 1440, height: 900 })
    await expect(page.locator('.workspace > .topbar')).toBeVisible()
    await expect(page.locator('.topbar .language-value')).toBeHidden()
    await expect(page.locator('.topbar .language-value')).toHaveText('English')
    await expect(page.locator('.topbar .language-button-icon')).toBeVisible()
    await page.setViewportSize({ width, height: 844 })
    await expect(topbar).toBeVisible()
    await expect(header.locator('.language-switcher .p-select')).toHaveAttribute('title', /English/)
    await page.route('**/api/v1/auth/logout', (route) => route.fulfill({ status: 204 }))
    await header.locator('.avatar').click()
    await page.getByRole('menuitem', { name: 'Log out', exact: true }).click()
    await expect(page).toHaveURL('/login')
    await expect(page.locator('.language-switcher .p-select')).toHaveAttribute('title', /English/)
    expect(errors).toEqual([])
  })
}

for (const viewport of [
  { width: 1440, height: 900 },
  { width: 701, height: 406 },
  { width: 390, height: 844 },
]) {
  test(`avatar menu keeps account actions hidden until opened at ${viewport.width}px`, async ({
    page,
  }) => {
    await page.setViewportSize(viewport)
    await signInWithRows(page)
    const avatar = page.getByRole('button', { name: '账号菜单', exact: true })
    await expect(avatar).toHaveAttribute('aria-expanded', 'false')
    await expect(page.getByRole('menuitem')).toHaveCount(0)
    await avatar.focus()
    await avatar.press('Enter')
    const menu = page.getByRole('menu', { name: '账号菜单', exact: true })
    await expect(menu).toBeVisible()
    await expect(avatar).toHaveAttribute('aria-expanded', 'true')
    await expect(menu.getByRole('menuitem')).toHaveCount(2)
    const arrow = await page.locator('.account-menu').evaluate((element) => {
      const rect = element.getBoundingClientRect()
      const trigger = document.querySelector('.avatar')!.getBoundingClientRect()
      const style = getComputedStyle(element, '::before')
      return {
        x: rect.left + parseFloat(style.left),
        anchor: trigger.left + trigger.width / 2,
        width: parseFloat(style.width),
        background: style.backgroundColor,
        surface: getComputedStyle(element).backgroundColor,
        left: rect.left,
        right: rect.right,
      }
    })
    expect(Math.abs(arrow.x - arrow.anchor)).toBeLessThan(2)
    expect(arrow.width).toBeLessThan(9)
    expect(arrow.width).toBeGreaterThan(5)
    expect(arrow.background).toBe(arrow.surface)
    expect(arrow.left).toBeGreaterThanOrEqual(0)
    expect(arrow.right).toBeLessThanOrEqual(viewport.width)
    const centered = arrow.anchor - (arrow.right - arrow.left) / 2
    if (centered >= 8 && centered + (arrow.right - arrow.left) <= viewport.width - 8) {
      expect(Math.abs((arrow.left + arrow.right) / 2 - arrow.anchor)).toBeLessThan(2)
    }
    await page.screenshot({ path: `test-results/account-menu-${viewport.width}.png` })
    await page.keyboard.press('Escape')
    await expect(menu).toHaveCount(0)
    await expect(avatar).toBeFocused()
    await avatar.click()
    await page.locator('.brand').click()
    await expect(menu).toHaveCount(0)
    await avatar.click()
    await page.getByRole('menuitem', { name: '修改密码', exact: true }).click()
    await expect(page).toHaveURL('/change-password')
    await expect(menu).toHaveCount(0)
    let logoutWrites = 0
    await page.route('**/api/v1/auth/logout', async (route) => {
      logoutWrites += 1
      await route.fulfill(logoutWrites === 1 ? { status: 503, json: {} } : { status: 204 })
    })
    await avatar.click()
    await page.getByRole('menuitem', { name: '退出', exact: true }).click()
    await expect(page.locator('.main > .alert.error')).toBeVisible()
    await expect(avatar).toBeEnabled()
    await expect(page).toHaveURL('/change-password')
    await avatar.click()
    await page.getByRole('menuitem', { name: '退出', exact: true }).click()
    await expect(page).toHaveURL('/login')
    await expect(page.getByRole('menu')).toHaveCount(0)
    expect(logoutWrites).toBe(2)
  })
}

test('language focus surrounds the complete component and effects start disabled', async ({
  page,
}) => {
  await page.goto('/login')
  const select = page.locator('.language-switcher').getByRole('combobox')
  await expect(select.locator('.language-button-icon')).toBeVisible()
  await expect(select.locator('.language-value')).toBeHidden()
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
  await expect(page.locator('.p-select-overlay')).toHaveClass(/lumina-popup-arrow/)
  await page.screenshot({ path: 'test-results/language-popup-refined.png' })
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
        await expect(overlay).toHaveClass(/lumina-popup-arrow/)
        const clipping = await overlay.evaluate((element) => {
          const style = getComputedStyle(element)
          const scroller = element.querySelector('.p-select-list-container')!
          return {
            overflowX: style.overflowX,
            overflowY: style.overflowY,
            radius: style.borderTopRightRadius,
            scrollRadius: getComputedStyle(scroller).borderTopRightRadius,
            clip: getComputedStyle(scroller).clipPath,
          }
        })
        expect(clipping.overflowX).toBe('visible')
        expect(clipping.overflowY).toBe('visible')
        expect(clipping.clip).not.toBe('none')
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
        // Editable values are rendered inside native input, not DOM text nodes;
        // Range has empty bounds there. Check the input box instead of an empty range.
        const text =
          element instanceof HTMLInputElement
            ? element.getBoundingClientRect()
            : range.getBoundingClientRect()
        const root = element.closest('.p-select')!
        const rect = root.getBoundingClientRect()
        const arrow = (root.querySelector('.language-button-icon') ??
          root.querySelector('.p-select-dropdown'))!.getBoundingClientRect()
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

test('shell has no skip-content entry and retains keyboard navigation', async ({ page }) => {
  await signInWithRows(page)
  await expect(page.locator('.skip-link, a[href="#main"]')).toHaveCount(0)
  const navigation = page.locator('.sidebar nav a')
  await navigation.first().focus()
  await expect(navigation.first()).toBeFocused()
  await page.keyboard.press('Tab')
  await expect(navigation.nth(1)).toBeFocused()
  await expect(page.getByRole('link', { name: '跳到主要内容' })).toHaveCount(0)
})

test('native context menu is prevented on pages and teleported overlays', async ({ page }) => {
  const menuPrevented = async (target: Locator) => {
    // Listen after the app handler and observe an actual right-click event.
    const event = page.evaluate(
      () =>
        new Promise<boolean>((resolve) => {
          document.addEventListener('contextmenu', (event) => resolve(event.defaultPrevented), {
            once: true,
          })
        }),
    )
    await target.click({ button: 'right' })
    expect(await event).toBe(true)
  }
  await page.goto('/login')
  await menuPrevented(page.locator('input[name="identifier"]'))
  await signInWithRows(page)
  await menuPrevented(page.locator('.page-heading h1'))
  await page.getByRole('button', { name: '创建用户' }).click()
  const dialog = page.getByRole('dialog')
  await menuPrevented(dialog.locator('.p-dialog-title'))
  await dialog.getByRole('combobox', { name: '角色', exact: true }).click()
  await menuPrevented(
    page.locator('.p-select-overlay').getByRole('option', { name: '工程师', exact: true }),
  )
  await dialog.locator('.p-dialog-title').click()
  await expect(page.locator('.p-select-overlay')).toHaveCount(0)
  await dialog.getByRole('button', { name: '取消', exact: true }).click()
  await expect(dialog).toHaveCount(0)
})

test('compact appearance controls work before and after login and persist only preferences', async ({
  page,
}) => {
  await page.goto('/login')
  const theme = page.getByRole('switch', { name: '深色主题', exact: true })
  const performance = page.getByRole('switch', { name: '性能模式', exact: true })
  await expect(theme).toHaveAttribute('aria-checked', 'false')
  await expect(performance).toHaveAttribute('aria-checked', 'true')
  const width = await page.locator('.language-switcher .p-select').evaluate((element) => {
    const k = Number(getComputedStyle(document.documentElement).getPropertyValue('--k'))
    return element.getBoundingClientRect().width / k
  })
  expect(width).toBeLessThan(95)
  await theme.click()
  await expect(page.locator('html')).toHaveAttribute('data-theme', 'dark')
  await performance.click()
  await expect(page.locator('html')).toHaveAttribute('data-fx', 'full')
  await page.reload()
  await expect(theme).toHaveAttribute('aria-checked', 'true')
  await expect(performance).toHaveAttribute('aria-checked', 'false')
  await expect(page.locator('html')).toHaveAttribute('data-theme', 'dark')
  await expect(page.locator('html')).toHaveAttribute('data-fx', 'full')
  await performance.click()
  await expect(page.locator('html')).toHaveAttribute('data-fx', 'lite')
  expect(await page.evaluate(() => document.getAnimations().length)).toBe(0)
  await signInWithRows(page)
  await expect(
    page.locator('.topbar').getByRole('switch', { name: '深色主题', exact: true }),
  ).toHaveAttribute('aria-checked', 'true')
  await expect(
    page.locator('.topbar').getByRole('switch', { name: '性能模式', exact: true }),
  ).toHaveAttribute('aria-checked', 'true')
  await theme.click()
  await expect(page.locator('html')).toHaveAttribute('data-theme', 'light')
  expect(await page.evaluate(() => Object.keys(localStorage).sort())).toEqual([
    'szx-admin-effects',
    'szx-admin-theme',
  ])
  await page.screenshot({ path: 'test-results/appearance-controls-desktop.png' })
})

test('appearance preferences tolerate blocked storage and honor reduced motion', async ({
  page,
}) => {
  await page.emulateMedia({ reducedMotion: 'reduce' })
  await page.addInitScript(() => {
    const get = Storage.prototype.getItem
    Storage.prototype.getItem = function (key) {
      if (key.startsWith('szx-admin-') && key !== 'szx-admin-locale')
        throw new DOMException('Blocked', 'SecurityError')
      return get.call(this, key)
    }
    Storage.prototype.setItem = function () {
      throw new DOMException('Blocked', 'SecurityError')
    }
  })
  await page.goto('/login')
  await page.getByRole('switch', { name: '深色主题', exact: true }).click()
  await expect(page.locator('html')).toHaveAttribute('data-theme', 'dark')
  await page.getByRole('switch', { name: '性能模式', exact: true }).click()
  await expect(page.locator('html')).toHaveAttribute('data-fx', 'full')
  expect(await page.evaluate(() => document.getAnimations().length)).toBe(0)
})

test('dark hover and active states stay dark with readable foregrounds', async ({ page }) => {
  await signInWithRows(page)
  await page.getByRole('switch', { name: '深色主题', exact: true }).click()
  const hover = page.locator('.sidebar a[href="/home"]')
  await hover.hover()
  expect(await hover.evaluate((element) => getComputedStyle(element).backgroundColor)).toBe(
    'rgb(38, 43, 69)',
  )
  const selected = page.locator('.sidebar a[href="/users"]')
  const activeGradient = await selected.evaluate(
    (element) => getComputedStyle(element).backgroundImage,
  )
  expect(activeGradient).toContain('rgb(35, 42, 115)')
  expect(activeGradient).toContain('rgb(63, 26, 102)')
  const palette = async (selector: string) =>
    page.locator(selector).evaluate((element) => {
      const style = getComputedStyle(element)
      return { background: style.backgroundImage, color: style.color }
    })
  expect(await palette('.avatar')).toEqual(await palette('.sidebar .brand-mark'))
  expect((await palette('.avatar')).color).toBe('rgb(255, 255, 255)')
  await hover.click()
  await expect(page).toHaveURL('/home')
  const secondary = page.locator('.main a[href="/change-password"]')
  await secondary.hover()
  const appearance = await secondary.evaluate((element) => {
    const style = getComputedStyle(element)
    return {
      background: style.backgroundColor,
      color: style.color,
      border: style.borderColor,
      shadow: style.boxShadow,
    }
  })
  expect(appearance).toMatchObject({
    background: 'rgb(38, 43, 69)',
    color: 'rgb(233, 236, 255)',
    border: 'rgba(0, 0, 0, 0)',
  })
  expect(appearance.shadow).toContain('rgb(53, 64, 204)')
  await page.screenshot({ path: 'test-results/dark-hover-states.png' })
  await page.getByRole('switch', { name: '深色主题', exact: true }).click()
  await secondary.hover()
  expect(await secondary.evaluate((element) => getComputedStyle(element).backgroundColor)).toBe(
    'rgb(238, 240, 255)',
  )
})

test.describe('circular controls at fractional display scale', () => {
  test.use({ deviceScaleFactor: 1.25 })
  test('dark circular controls use physical-pixel sizing and smooth SVG strokes', async ({
    page,
  }) => {
    await signInWithRows(page)
    await page.getByRole('switch', { name: '深色主题', exact: true }).click()
    const metrics = await page.locator('.lumina-preference-button').evaluateAll((elements) =>
      elements.map((element) => {
        const rect = element.getBoundingClientRect()
        const style = getComputedStyle(element)
        const icon = element.querySelector('svg')!
        const iconRect = icon.getBoundingClientRect()
        const path = icon.querySelector('path')!
        return {
          width: rect.width * devicePixelRatio,
          height: rect.height * devicePixelRatio,
          iconWidth: iconRect.width * devicePixelRatio,
          border: style.borderColor,
          shadow: style.boxShadow,
          linecap: getComputedStyle(path).strokeLinecap,
          vector: getComputedStyle(path).vectorEffect,
          rendering: icon.getAttribute('shape-rendering'),
        }
      }),
    )
    for (const metric of metrics) {
      expect(Math.abs(metric.width - Math.round(metric.width))).toBeLessThan(0.03)
      expect(Math.abs(metric.height - Math.round(metric.height))).toBeLessThan(0.03)
      expect(Math.abs(metric.iconWidth - Math.round(metric.iconWidth))).toBeLessThan(0.03)
      expect(metric.border).toBe('rgba(0, 0, 0, 0)')
      expect(metric.shadow).not.toBe('none')
      expect(metric.linecap).toBe('round')
      expect(metric.vector).toBe('non-scaling-stroke')
      expect(metric.rendering).toBe('geometricPrecision')
    }
    await expect(page.locator('.brand-mark.lumina-mark')).toHaveText('S')
    await expect(page.locator('.avatar.lumina-mark')).toHaveText('系')
    await page.screenshot({ path: 'test-results/dark-controls-fractional-scale.png' })
  })
})

for (const ratio of [1, 1.25, 1.5, 2]) {
  test.describe(`smooth hover strokes at DPR ${ratio}`, () => {
    test.use({ deviceScaleFactor: ratio })
    for (const theme of ['light', 'dark']) {
      test(`${theme} hover strokes are feathered, DPI-scaled and layout stable`, async ({
        page,
      }) => {
        await signInWithRows(page)
        if (theme === 'dark')
          await page.getByRole('switch', { name: '深色主题', exact: true }).click()
        const controls = [
          page.locator('.language-switcher'),
          page.getByRole('combobox', { name: '角色', exact: true }).locator('..'),
          page.getByRole('searchbox', { name: '搜索用户', exact: true }),
          page.locator('.table-heading button'),
          page.locator('.sidebar a[href="/users"]'),
          page.locator('.lumina-preference-button').first(),
        ]
        for (const control of controls) {
          const before = await control.boundingBox()
          await control.hover()
          const after = await control.boundingBox()
          expect(after).toEqual(before)
          const stroke = await control.evaluate((element) => {
            const style = getComputedStyle(element)
            return {
              border: style.borderColor,
              shadow: style.boxShadow,
              width: parseFloat(style.getPropertyValue('--lg-edge-px')) * devicePixelRatio,
              feather: parseFloat(style.getPropertyValue('--lg-edge-blur')) * devicePixelRatio,
            }
          })
          expect(stroke.border).toBe('rgba(0, 0, 0, 0)')
          expect(stroke.shadow).toContain('inset')
          expect(stroke.width).toBeCloseTo(ratio, 4)
          expect(stroke.feather).toBeCloseTo(1.25 * ratio, 4)
        }
        const role = page.getByRole('combobox', { name: '角色', exact: true })
        await role.focus()
        const focus = await role
          .locator('..')
          .evaluate((element) => getComputedStyle(element).boxShadow)
        expect(focus).toContain('inset')
        expect(focus).toContain('rgba(91, 108, 255,')
        await expect(role).toHaveText('全部角色')
        await page.screenshot({ path: `test-results/smooth-hover-${theme}-${ratio}.png` })
      })
    }
  })
}

for (const ratio of [1, 1.25, 1.5, 2]) {
  test.describe(`rendered result-select edge at DPR ${ratio}`, () => {
    test.use({ deviceScaleFactor: ratio })
    test('actual corner pixels transition more smoothly than the former hairline', async ({
      page,
    }) => {
      await signInWithRows(page)
      await page.getByRole('switch', { name: '深色主题', exact: true }).click()
      await page.getByRole('link', { name: '审计日志', exact: true }).click()
      const select = page.getByRole('combobox', { name: '结果', exact: true }).locator('..')
      await select.hover()
      const cornerMetrics = async () => {
        const dimensions = await select.evaluate((element) => ({
          width: element.getBoundingClientRect().width,
          radius: parseFloat(getComputedStyle(element).borderTopLeftRadius),
        }))
        const png = (await select.screenshot()).toString('base64')
        return page.evaluate(
          async ({ png, dimensions }) => {
            const image = new Image()
            image.src = `data:image/png;base64,${png}`
            await image.decode()
            const canvas = document.createElement('canvas')
            canvas.width = image.naturalWidth
            canvas.height = image.naturalHeight
            const context = canvas.getContext('2d')!
            context.drawImage(image, 0, 0)
            const radius = Math.floor((dimensions.radius * image.naturalWidth) / dimensions.width)
            const data = context.getImageData(0, 0, radius, radius).data
            let peak = 0
            let jump = 0
            const blue = (x: number, y: number) => data[(y * radius + x) * 4 + 2]!
            const brightness = (x: number, y: number) => {
              const index = (y * radius + x) * 4
              return data[index]! * 0.2126 + data[index + 1]! * 0.7152 + data[index + 2]! * 0.0722
            }
            for (let y = 0; y < radius; y++) {
              for (let x = 0; x < radius; x++) {
                peak = Math.max(peak, blue(x, y))
                if (x + 1 < radius)
                  jump = Math.max(jump, Math.abs(brightness(x, y) - brightness(x + 1, y)))
                if (y + 1 < radius)
                  jump = Math.max(jump, Math.abs(brightness(x, y) - brightness(x, y + 1)))
              }
            }
            return { peak, jump }
          },
          { png, dimensions },
        )
      }
      // Reproduce the previous hard, one-physical-pixel hover edge on the same element.
      await select.evaluate((element) => {
        const node = element as HTMLElement
        node.style.setProperty(
          'box-shadow',
          `inset 0 0 ${0.5 / devicePixelRatio}px ${1 / devicePixelRatio}px rgb(163, 170, 255)`,
          'important',
        )
      })
      const former = await cornerMetrics()
      await select.evaluate((element) =>
        (element as HTMLElement).style.removeProperty('box-shadow'),
      )
      const current = await cornerMetrics()
      expect(current.jump).toBeLessThan(former.jump * 0.85)
      expect(current.peak).toBeLessThan(former.peak)
      expect(current.peak).toBeGreaterThan(75) // A visible blue hover indicator remains.
      await page.screenshot({ path: `test-results/result-hover-rendered-${ratio}.png` })
    })
  })
}

test('login glass surface uses the reusable panel and respects the performance switch', async ({
  page,
}) => {
  await page.goto('/login')
  const panel = page.locator('.login-card.lumina-panel')
  await expect(panel).toBeVisible()
  const before = await panel.boundingBox()
  expect(await panel.evaluate((element) => getComputedStyle(element).backdropFilter)).toBe('none')
  await page.getByRole('switch', { name: '性能模式', exact: true }).click()
  expect(await panel.evaluate((element) => getComputedStyle(element).backdropFilter)).toContain(
    'blur(',
  )
  expect(await panel.boundingBox()).toEqual(before)
  await page.getByRole('switch', { name: '性能模式', exact: true }).click()
  expect(await panel.evaluate((element) => getComputedStyle(element).backdropFilter)).toBe('none')
})

test('dark password information notice keeps a dark semantic surface and readable text', async ({
  page,
}) => {
  await signInWithRows(page)
  await page.getByRole('switch', { name: '深色主题', exact: true }).click()
  await page.locator('.topbar .avatar').click()
  await page.getByRole('menuitem', { name: '修改密码', exact: true }).click()
  const notice = page.locator('.form-stack .alert.info')
  await expect(notice).toContainText('修改密码后')
  expect(
    await notice.evaluate((element) => ({
      background: getComputedStyle(element).backgroundColor,
      color: getComputedStyle(element).color,
      border: getComputedStyle(element).borderColor,
    })),
  ).toEqual({
    background: 'rgba(91, 108, 255, 0.14)',
    color: 'rgb(197, 202, 255)',
    border: 'rgba(125, 139, 255, 0.36)',
  })
  await page.screenshot({ path: 'test-results/dark-password-notice.png' })
  await page.getByRole('switch', { name: '深色主题', exact: true }).click()
  expect(await notice.evaluate((element) => getComputedStyle(element).backgroundColor)).toBe(
    'rgb(238, 240, 255)',
  )
})

test('login, authenticated shell and logout retain one appearance and language state', async ({
  page,
}) => {
  await page.goto('/login')
  await page.getByRole('switch', { name: '深色主题', exact: true }).click()
  await page.getByRole('switch', { name: '性能模式', exact: true }).click()
  await chooseOption(page, page.locator('.language-switcher').getByRole('combobox'), 'English')
  await signInWithRows(page)
  await expect(
    page.locator('.topbar').getByRole('switch', { name: 'Dark theme', exact: true }),
  ).toHaveAttribute('aria-checked', 'true')
  await expect(
    page.locator('.topbar').getByRole('switch', { name: 'Performance mode', exact: true }),
  ).toHaveAttribute('aria-checked', 'false')
  await expect(page.locator('html')).toHaveAttribute('data-theme', 'dark')
  await expect(page.locator('html')).toHaveAttribute('data-fx', 'full')
  await expect(page.locator('.language-switcher').getByRole('combobox')).toHaveText('English')
  await page.getByRole('switch', { name: 'Dark theme', exact: true }).click()
  await page.getByRole('switch', { name: 'Performance mode', exact: true }).click()
  await page.route('**/api/v1/auth/logout', (route) => route.fulfill({ status: 204 }))
  await page.locator('.topbar .avatar').click()
  await page.getByRole('menuitem', { name: 'Log out', exact: true }).click()
  await expect(page).toHaveURL('/login')
  await expect(page.getByRole('switch', { name: 'Dark theme', exact: true })).toHaveAttribute(
    'aria-checked',
    'false',
  )
  await expect(page.getByRole('switch', { name: 'Performance mode', exact: true })).toHaveAttribute(
    'aria-checked',
    'true',
  )
  await expect(page.locator('html')).toHaveAttribute('data-theme', 'light')
  await expect(page.locator('html')).toHaveAttribute('data-fx', 'lite')
  await expect(page.locator('.language-switcher').getByRole('combobox')).toHaveText('English')
})

test('all admin scroll regions share Lumina styling including body portals and future content', async ({
  page,
}) => {
  await page.setViewportSize({ width: 1024, height: 720 })
  await signInWithRows(page)
  await expect(page.locator('html')).toHaveAttribute('data-lumina-scrollbars', '')
  const verify = async (region: Locator) => {
    const style = await region.evaluate((element) => ({
      width: getComputedStyle(element, '::-webkit-scrollbar').width,
      height: getComputedStyle(element, '::-webkit-scrollbar').height,
      radius: getComputedStyle(element, '::-webkit-scrollbar-thumb').borderRadius,
      track: getComputedStyle(element, '::-webkit-scrollbar-track').backgroundColor,
      buttons: getComputedStyle(element, '::-webkit-scrollbar-button').display,
    }))
    expect(style).toMatchObject({
      width: '10px',
      height: '10px',
      radius: '999px',
      track: 'rgba(0, 0, 0, 0)',
      buttons: 'none',
    })
  }
  await verify(page.locator('.sidebar'))
  await verify(page.locator('.table-scroll'))
  await verify(page.locator('.main'))
  const table = page.locator('.table-scroll')
  expect(await table.evaluate((element) => element.scrollHeight > element.clientHeight)).toBe(true)
  await table.hover()
  await page.mouse.wheel(0, 400)
  await expect.poll(() => table.evaluate((element) => element.scrollTop)).toBeGreaterThan(0)
  await page.getByRole('link', { name: '审计日志' }).click()
  const result = page.getByRole('combobox', { name: '结果', exact: true })
  await result.click()
  const list = page.locator('.p-select-list-container')
  await expect(list).not.toHaveClass(/lumina-scrollbar/)
  await verify(list)
  expect(await list.evaluate((element) => Boolean(element.closest('#app')))).toBe(false)
  await result.press('Escape')
  await page.getByRole('button', { name: '查看详情', exact: true }).first().click()
  const content = page.locator('.p-dialog-content')
  await expect(content).toBeVisible()
  await verify(content)
  expect(await content.evaluate((element) => Boolean(element.closest('#app')))).toBe(false)
  // A future/custom widget added after installation also receives the scope automatically.
  await content.evaluate((element) => {
    const pre = document.createElement('pre')
    pre.dataset.testid = 'future-details-scroll'
    pre.style.cssText = 'overflow:auto;max-height:80px;max-width:200px'
    pre.textContent = Array.from({ length: 30 }, () => 'x'.repeat(120)).join('\n')
    element.append(pre)
  })
  const future = page.getByTestId('future-details-scroll')
  await verify(future)
  expect(await future.evaluate((element) => getComputedStyle(element).scrollbarGutter)).toBe('auto')
  const light = await future.evaluate(
    (element) => getComputedStyle(element, '::-webkit-scrollbar-thumb').backgroundColor,
  )
  await page.evaluate(() => {
    document.documentElement.dataset.theme = 'dark'
  })
  expect(
    await future.evaluate(
      (element) => getComputedStyle(element, '::-webkit-scrollbar-thumb').backgroundColor,
    ),
  ).not.toBe(light)
})
