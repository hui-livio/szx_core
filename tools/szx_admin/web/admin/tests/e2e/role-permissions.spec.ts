import { test, expect, type Page } from '@playwright/test'
import type { PermissionRecord, RoleRecord } from '../../src/api/types'

async function setup(page: Page, rights = ['role.read', 'permission.read'], extraPermissions = 0) {
  const writes: string[] = []
  const reads: string[] = []
  let conflict = false
  let roleReads = 0
  let failMetadata = false
  const permissions: PermissionRecord[] = [
    {
      id: 'business',
      code: 'business.read',
      name: '业务读取',
      description: '',
      module: 'business',
      source: 'system',
      enforcement: 'server',
      status: 'active',
      required_for_super_admin: true,
      version: 1,
    },
    {
      id: 'export',
      code: 'report.export',
      name: '导出报表',
      description: '',
      module: 'reports',
      source: 'custom',
      enforcement: 'client',
      status: 'active',
      required_for_super_admin: false,
      version: 1,
    },
    {
      id: 'disabled',
      code: 'report.disabled',
      name: '停用报表',
      description: '',
      module: 'reports',
      source: 'custom',
      enforcement: 'client',
      status: 'disabled',
      required_for_super_admin: false,
      version: 1,
    },
  ]
  const roles: RoleRecord[] = [
    {
      id: 'operator',
      code: 'operator',
      name: '操作员',
      description: '',
      system_key: 'operator',
      status: 'active',
      version: 2,
      authz_version: 2,
    },
    {
      id: 'engineer',
      code: 'engineer',
      name: '工程师',
      description: '',
      system_key: 'engineer',
      status: 'active',
      version: 3,
      authz_version: 3,
    },
    {
      id: 'disabled-role',
      code: 'viewer',
      name: '停用角色',
      description: '',
      system_key: null,
      status: 'disabled',
      version: 1,
      authz_version: 1,
    },
    {
      id: 'archived-role',
      code: 'legacy',
      name: '归档角色',
      description: '',
      system_key: null,
      status: 'archived',
      version: 1,
      authz_version: 1,
    },
  ]
  const ids: Record<string, string[]> = {
    operator: ['business', 'export', 'disabled'],
    engineer: [],
    'disabled-role': ['export'],
    'archived-role': ['business'],
  }
  for (let index = 0; index < extraPermissions; index++) {
    const permission: PermissionRecord = {
      ...permissions[1]!,
      id: `extra-${index}`,
      code: `report.extra_${index}`,
      name: `额外权限 ${index}`,
    }
    permissions.push(permission)
    ids.operator!.push(permission.id)
  }
  const user = {
    id: 'admin',
    login_identifier: 'admin',
    display_name: 'Admin',
    role: 'super_admin',
    status: 'active',
    force_password_change: false,
    password_change_recommended: false,
    version: 1,
    authz_version: 1,
    created_at: 0,
    updated_at: 0,
    last_login_at: null,
  }
  await page.route('**/api/v1/**', async (route) => {
    const request = route.request()
    const path = new URL(request.url()).pathname.replace('/api/v1', '')
    const reply = (data: unknown) => route.fulfill({ json: { data, request_id: 'role-view' } })
    if (path === '/auth/login')
      return reply({
        user,
        access_token: 'token',
        token_type: 'Bearer',
        csrf_token: 'csrf',
        expires_at: Date.now() + 900_000,
        expires_in: 900,
        authz_version: 1,
      })
    if (request.method() !== 'GET') writes.push(path)
    else reads.push(path)
    if (path === '/client/me/permissions')
      return reply({ user_id: user.id, role: user.role, authz_version: 1, capabilities: rights })
    if (path === '/admin/roles') {
      const include = new URL(request.url()).searchParams.get('include_archived') === 'true'
      return reply({ items: roles.filter((role) => include || role.status !== 'archived') })
    }
    if (path === '/admin/permissions') {
      if (failMetadata) return route.fulfill({ status: 503, json: { code: 2216 } })
      if (!rights.includes('permission.read'))
        return route.fulfill({ status: 403, json: { code: 2204 } })
      return reply({ items: permissions })
    }
    if (path.startsWith('/admin/roles/')) {
      const id = path.split('/')[3]!
      if (path.endsWith('/permissions')) return reply({ permission_ids: ids[id] ?? [] })
      const role = roles.find((value) => value.id === id)!
      if (conflict && id === 'operator' && ++roleReads === 2) {
        conflict = false
        role.version++
        role.authz_version++
      }
      return reply({ ...role })
    }
    return reply({ items: [], total: 0, page: 1, page_size: 20 })
  })
  await page.goto('/login')
  await page.locator('input[name="identifier"]').fill('admin')
  await page.locator('input[name="password"]').fill('TestPassword123')
  await page.locator('.login-submit').click()
  await expect(page).toHaveURL('/home')
  return {
    writes,
    reads,
    conflict: () => {
      conflict = true
      roleReads = 0
    },
    disableExport: () => {
      permissions[1]!.status = 'disabled'
      roles[0]!.version++
      roles[0]!.authz_version++
    },
    failMetadata: () => {
      failMetadata = true
    },
  }
}
async function selectRole(page: Page, name: string) {
  await page.getByRole('combobox', { name: '选择角色', exact: true }).click()
  await page.getByRole('option', { name, exact: true }).click()
}
async function openAssociations(page: Page) {
  await page.locator('.sidebar nav a[href="/roles"]').click()
  await page.getByRole('link', { name: '查看角色权限关联', exact: true }).click()
  await expect(page).toHaveURL('/roles/permissions')
}

test('home and sidebar provide a read-only view of all role permission assignments', async ({
  page,
}) => {
  const state = await setup(page)
  await expect(page.locator('.home-navigation a[href="/roles"]')).toBeVisible()
  await openAssociations(page)
  await expect(page.locator('.role-permissions-summary')).toContainText('关联权限：3')
  await expect(page.locator('.role-permissions-summary')).toContainText('生效权限：2')
  await expect(page.getByRole('row').filter({ hasText: 'business.read' })).toContainText('内置')
  await expect(page.getByRole('row').filter({ hasText: 'report.export' })).toContainText('客户端')
  await expect(page.getByRole('row').filter({ hasText: 'report.disabled' })).toContainText('不生效')
  await expect(page.getByRole('button', { name: '确认', exact: true })).toHaveCount(0)
  await selectRole(page, '停用角色')
  await expect(page.locator('.role-permissions-summary')).toContainText('生效权限：0')
  await expect(page.getByRole('row').filter({ hasText: 'report.export' })).toContainText('不生效')
  await selectRole(page, '工程师')
  await expect(page.getByText('该角色没有关联权限。', { exact: true })).toBeVisible()
  await page.getByLabel('包含已归档角色').check()
  await selectRole(page, '归档角色')
  await expect(page.locator('.role-permissions-summary')).toContainText('关联权限：1')
  expect(state.writes).toEqual([])
})

test('role.read alone can inspect IDs without permission.read or assignment rights', async ({
  page,
}) => {
  const state = await setup(page, ['role.read'])
  await openAssociations(page)
  await expect(page.locator('.role-permissions-summary')).toContainText('关联权限：3')
  await expect(page.getByText(/仅展示关联权限 ID/)).toBeVisible()
  await expect(page.getByRole('cell', { name: '无法确认', exact: true })).toHaveCount(3)
  expect(state.reads).not.toContain('/admin/permissions')
  expect(state.writes).toEqual([])
})

test('quick role switching cannot publish a stale assignment response', async ({ page }) => {
  await setup(page)
  let release!: () => void
  let arrived!: () => void
  const blocked = new Promise<void>((resolve) => {
    release = resolve
  })
  const started = new Promise<void>((resolve) => {
    arrived = resolve
  })
  await page.route('**/api/v1/admin/roles/operator/permissions', async (route) => {
    arrived()
    await blocked
    await route.fallback().catch(() => {})
  })
  await openAssociations(page)
  await started
  await selectRole(page, '工程师')
  await expect(page.locator('.role-permissions-summary')).toContainText('工程师')
  release()
  await page.waitForTimeout(150)
  await expect(page.locator('.role-permissions-summary')).toContainText('关联权限：0')
  await expect(page.getByText('report.export', { exact: true })).toHaveCount(0)
})

test('refresh re-reads permission state and detects role version races', async ({ page }) => {
  const state = await setup(page)
  await openAssociations(page)
  await expect(page.locator('.role-permissions-summary')).toContainText('生效权限：2')
  state.disableExport()
  await page.getByRole('button', { name: '刷新', exact: true }).click()
  await expect(page.locator('.role-permissions-summary')).toContainText('生效权限：1')
  state.conflict()
  await page.getByRole('button', { name: '刷新', exact: true }).click()
  await expect(page.getByRole('alert')).toContainText('2209')
  await expect(page.locator('.role-permissions-summary')).toHaveCount(0)
  await page.getByRole('button', { name: '重新加载', exact: true }).click()
  await expect(page.locator('.role-permissions-summary')).toContainText('生效权限：1')
})

test('without role.read neither home/sidebar links nor direct routing expose the view', async ({
  page,
}) => {
  const state = await setup(page, ['permission.read'])
  await expect(page.locator('.sidebar nav a[href="/roles"]')).toHaveCount(0)
  await expect(page.locator('.home-navigation a[href="/roles"]')).toHaveCount(0)
  await page.evaluate(() => {
    history.pushState({}, '', '/roles/permissions')
    window.dispatchEvent(new PopStateEvent('popstate'))
  })
  await expect(page).toHaveURL('/home')
  expect(state.reads).not.toContain('/admin/roles')
})

test('dark mobile view keeps scrolling and metadata errors fall back to IDs', async ({ page }) => {
  await page.setViewportSize({ width: 390, height: 844 })
  const errors: string[] = []
  page.on('pageerror', (error) => errors.push(error.message))
  const state = await setup(page)
  await openAssociations(page)
  await expect(page.locator('.role-permissions-summary')).toContainText('关联权限：3')
  await page.evaluate(() => {
    document.documentElement.dataset.theme = 'dark'
  })
  state.failMetadata()
  await page.getByRole('button', { name: '刷新', exact: true }).click()
  await expect(page.getByRole('alert')).toContainText('2216')
  await expect(page.locator('.role-permissions-summary')).toContainText('生效权限：—')
  expect(await page.evaluate(() => document.documentElement.scrollWidth)).toBeLessThanOrEqual(390)
  expect(errors).toEqual([])
})

test('sidebar tab order matches the home page shortcut order', async ({ page }) => {
  await setup(page, [
    'user.read',
    'role.read',
    'permission.read',
    'audit.read',
    'system.metrics.read',
  ])
  const paths = ['/home', '/users', '/roles', '/permissions', '/logs', '/system/performance']
  const navigation = page.locator('.sidebar nav a')
  await expect(navigation).toHaveCount(paths.length)
  expect(
    await navigation.evaluateAll((elements) =>
      elements.map((element) => element.getAttribute('href')),
    ),
  ).toEqual(paths)
  expect(
    await page
      .locator('.home-navigation a')
      .evaluateAll((elements) => elements.map((element) => element.getAttribute('href'))),
  ).toEqual([...paths.slice(1), '/change-password'])
  await navigation.first().focus()
  for (const path of paths.slice(1)) {
    await page.keyboard.press('Tab')
    await expect(page.locator(`.sidebar nav a[href="${path}"]`)).toBeFocused()
  }
})

for (const viewport of [
  { width: 1440, height: 900 },
  { width: 390, height: 844 },
  { width: 1280, height: 560 },
]) {
  test(`RBAC tables contain wheel scrolling at ${viewport.width}×${viewport.height}`, async ({
    page,
  }) => {
    await page.setViewportSize(viewport)
    await setup(page, ['role.read', 'permission.read'], 60)
    for (const path of ['/roles/permissions', '/permissions']) {
      if (path === '/roles/permissions') await openAssociations(page)
      else await page.locator(`.sidebar nav a[href="${path}"]`).click()
      await expect(page).toHaveURL(path)
      await expect(
        page.locator(
          path === '/roles/permissions' ? '.role-permissions-summary' : '.permissions-page',
        ),
      ).toBeVisible()
      const table = page.locator('.table-scroll')
      await expect(table.locator('tbody tr')).toHaveCount(63)
      await page.evaluate(() => document.fonts.ready)
      expect(await table.evaluate((element) => element.clientHeight)).toBeGreaterThan(70)
      expect(await table.evaluate((element) => element.scrollHeight > element.clientHeight)).toBe(
        true,
      )
      const fixed = page.locator('.topbar, .page-heading, .filters, .app-footer')
      const positions = await fixed.evaluateAll((elements) =>
        elements.map((element) => element.getBoundingClientRect().top),
      )
      await table.hover()
      await page.mouse.wheel(0, 800)
      await expect.poll(() => table.evaluate((element) => element.scrollTop)).toBeGreaterThan(0)
      expect(await page.locator('.main').evaluate((element) => element.scrollTop)).toBe(0)
      expect(
        await fixed.evaluateAll((elements) =>
          elements.map((element) => element.getBoundingClientRect().top),
        ),
      ).toEqual(positions)
      await table.evaluate((element) => {
        element.scrollTop = element.scrollHeight
      })
      await page.mouse.wheel(0, 800)
      expect(await page.locator('.main').evaluate((element) => element.scrollTop)).toBe(0)
      if (viewport.width === 390) {
        expect(await table.evaluate((element) => element.scrollWidth > element.clientWidth)).toBe(
          true,
        )
        await page.mouse.wheel(600, 0)
        await expect.poll(() => table.evaluate((element) => element.scrollLeft)).toBeGreaterThan(0)
      }
    }
  })
}

for (const [locale, title, permissionTitle, managementTitle] of [
  ['ja-JP', 'ロールと権限', '権限管理', 'ロール管理'],
  ['ko-KR', '역할 및 권한', '권한 관리', '역할 관리'],
  ['fr-FR', 'Rôles et permissions', 'Gestion des permissions', 'Gestion des rôles'],
  ['ru-RU', 'Роли и разрешения', 'Управление разрешениями', 'Управление ролями'],
]) {
  test(`RBAC pages use native translations in ${locale}`, async ({ page }) => {
    await page.addInitScript((value) => localStorage.setItem('szx-admin-locale', value!), locale)
    await setup(page)
    await expect(page.locator('.sidebar nav a[href="/roles"]')).toContainText(managementTitle!)
    await page.locator('.sidebar nav a[href="/roles"]').click()
    await page.locator('.roles-page a[href="/roles/permissions"]').click()
    await expect(page.locator('h1')).toHaveText(title!)
    await expect(page.locator('.role-permissions-summary')).toBeVisible()
    await page.locator('.sidebar nav a[href="/permissions"]').click()
    await expect(page.locator('h1')).toHaveText(permissionTitle!)
    await expect(page.locator('.table-scroll tbody tr')).toHaveCount(3)
  })
}
