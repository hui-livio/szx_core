import { expectThemedPanels } from './surface'
import { test, expect, type Page } from '@playwright/test'
import { expectHeaderAboveRowControls } from './table'
import type { RoleRecord } from '../../src/api/types'

const rights = [
  'role.read',
  'role.create',
  'role.update',
  'role.status',
  'role.archive',
  'role.permission.assign',
  'role.scope.assign',
  'permission.read',
  'user.read',
  'user.create',
  'user.role.assign',
]
async function setup(page: Page, capabilities = rights, actorRole = 'super_admin', extraRows = 0) {
  let activeCapabilities = [...capabilities]
  let expiredRead = false
  const writes: { method: string; path: string; data: unknown; version?: string }[] = []
  const rows: RoleRecord[] = [
    {
      id: 'root',
      code: 'super_admin',
      name: '超级管理员',
      description: '',
      system_key: 'super_admin',
      status: 'active',
      version: 1,
      authz_version: 1,
      user_count: 1,
      permission_count: 1,
      manageable: true,
    },
    {
      id: 'viewer',
      code: 'reviewer',
      name: '审核员',
      description: 'Review documents',
      system_key: null,
      status: 'active',
      version: 2,
      authz_version: 2,
      user_count: 0,
      permission_count: 3,
      manageable: true,
    },
    {
      id: 'outside',
      code: 'outside',
      name: '范围外角色',
      description: '',
      system_key: null,
      status: 'active',
      version: 1,
      authz_version: 1,
      user_count: 0,
      permission_count: 0,
      manageable: false,
    },
    {
      id: 'used',
      code: 'used',
      name: '使用中角色',
      description: '',
      system_key: null,
      status: 'active',
      version: 1,
      authz_version: 1,
      user_count: 2,
      permission_count: 0,
      manageable: true,
    },
  ]
  for (let i = 0; i < extraRows; i++) {
    rows.push({ ...rows[1]!, id: `extra-${i}`, code: `extra_${i}`, name: `额外角色 ${i}` })
  }
  const assignments: Record<string, string[]> = {
    root: ['system'],
    viewer: ['system', 'disabled', 'unknown'],
  }
  const scopes: Record<string, string[]> = { root: rows.map((role) => role.id), viewer: ['viewer'] }
  let conflict = false
  let refreshes = 0
  const user = {
    id: 'actor',
    login_identifier: 'admin',
    display_name: 'Admin',
    role: actorRole,
    status: 'active',
    force_password_change: false,
    password_change_recommended: false,
    version: 1,
    authz_version: 1,
    role_id: 'root',
    role_authz_version: 1,
    created_at: 0,
    updated_at: 0,
    last_login_at: null,
  }
  const session = {
    user,
    access_token: 'token',
    csrf_token: 'csrf',
    expires_at: Date.now() + 900000,
    expires_in: 900,
    authz_version: 1,
    token_type: 'Bearer',
  }
  await page.route('**/api/v1/**', async (route) => {
    const request = route.request()
    const path = new URL(request.url()).pathname.replace('/api/v1', '')
    const method = request.method()
    const data = request.postData() ? request.postDataJSON() : undefined
    const reply = (payload: unknown, status = 200) =>
      route.fulfill({ status, json: { data: payload, request_id: 'roles-test' } })
    const fail = (code: number, status = 409) =>
      route.fulfill({ status, json: { code, request_id: 'roles-error' } })
    if (path === '/auth/login') return reply(session)
    if (path === '/auth/refresh') {
      refreshes++
      return reply(session)
    }
    if (path === '/client/me/permissions')
      return reply({
        user_id: user.id,
        role: user.role,
        authz_version: 1,
        capabilities: activeCapabilities,
        manageable_roles: [
          ...rows
            .filter((role) => role.manageable && role.status !== 'archived')
            .map((role) => role.code),
          'inactive_reviewer',
        ],
        assignable_roles: rows
          .filter((role) => role.manageable && role.status === 'active')
          .map((role) => role.code),
      })
    if (method !== 'GET')
      writes.push({ method, path, data, version: request.headers()['if-match'] })
    if (path === '/admin/roles' && method === 'GET') {
      if (expiredRead) {
        expiredRead = false
        return fail(2200, 401)
      }
      return reply({
        items: rows.filter(
          (role) =>
            new URL(request.url()).searchParams.get('include_archived') === 'true' ||
            role.status !== 'archived',
        ),
      })
    }
    if (path === '/admin/roles' && method === 'POST') {
      if (rows.some((role) => role.code === data.code)) return fail(2227)
      const role: RoleRecord = {
        ...data,
        id: data.code,
        system_key: null,
        status: 'active',
        version: 1,
        authz_version: 1,
        user_count: 0,
        permission_count: 0,
        manageable: true,
      }
      rows.push(role)
      return reply(role, 201)
    }
    if (path === '/admin/permissions')
      return reply({
        items: [
          {
            id: 'system',
            code: 'business.read',
            name: '业务读取',
            description: '',
            module: 'business',
            source: 'system',
            enforcement: 'server',
            status: 'active',
            version: 1,
            required_for_super_admin: true,
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
            version: 1,
            required_for_super_admin: false,
          },
          {
            id: 'disabled',
            code: 'report.disabled',
            name: '已停用权限',
            description: '',
            module: 'reports',
            source: 'custom',
            enforcement: 'client',
            status: 'disabled',
            version: 1,
            required_for_super_admin: false,
          },
        ],
      })
    if (path.startsWith('/admin/roles/')) {
      const id = path.split('/')[3]!
      const role = rows.find((item) => item.id === id)!
      if (method !== 'GET') {
        if (conflict) {
          conflict = false
          role.version++
          return fail(2209)
        }
        if (request.headers()['if-match'] !== String(role.version)) return fail(2209)
        if (method === 'DELETE') {
          role.status = 'archived'
          role.version++
          assignments[id] = []
          scopes[id] = []
          return route.fulfill({ status: 204 })
        }
        if (path.endsWith('/permissions')) {
          assignments[id] = data.permission_ids
          role.permission_count = assignments[id]!.length
        } else if (path.endsWith('/manage-scopes')) scopes[id] = data.target_role_ids
        else Object.assign(role, data)
        role.version++
        role.authz_version++
        return reply(role)
      }
      if (path.endsWith('/permissions')) return reply({ permission_ids: assignments[id] ?? [] })
      if (path.endsWith('/manage-scopes')) return reply({ target_role_ids: scopes[id] ?? [] })
      return reply(role)
    }
    if (path === '/admin/users' && method === 'GET')
      return reply({ items: [], total: 0, page: 1, page_size: 20 })
    if (path === '/admin/users' && method === 'POST')
      return reply(
        {
          user: { ...user, ...data, id: 'created-user' },
          temporary_credential: null,
          temporary_credential_expires_at: null,
        },
        201,
      )
    return reply({ items: [] })
  })
  await page.goto('/login')
  await page.locator('input[name="identifier"]').fill('admin')
  await page.locator('input[name="password"]').fill('TestPassword123')
  await page.locator('.login-submit').click()
  await expect(page).toHaveURL('/home')
  return {
    writes,
    rows,
    assignments,
    scopes,
    conflict: () => {
      conflict = true
    },
    refreshes: () => refreshes,
    revokeWrites: () => {
      activeCapabilities = ['role.read', 'permission.read']
      user.role_authz_version++
      expiredRead = true
    },
  }
}
async function openRoles(page: Page) {
  await page.locator('.sidebar nav a[href="/roles"]').click()
  await expect(page.locator('.roles-page')).toBeVisible()
  await expect(page.locator('.roles-page .table-scroll tbody tr')).toHaveCount(4)
}
const row = (page: Page, code = 'reviewer') =>
  page.getByRole('row').filter({ has: page.getByText(code, { exact: true }) })

test('role create, immutable code, edit, status and archive confirmation are versioned', async ({
  page,
}) => {
  const state = await setup(page)
  await openRoles(page)
  await page.getByRole('button', { name: '新增角色' }).click()
  let dialog = page.getByRole('dialog')
  await dialog.getByLabel('角色代码', { exact: true }).fill('document_viewer')
  await dialog.getByLabel('角色名称', { exact: true }).fill('文档查看员')
  await dialog.getByLabel('说明', { exact: true }).fill('查看文档')
  await dialog.getByRole('button', { name: '确认', exact: true }).click()
  await expect(dialog).toHaveCount(0)
  await expect(row(page, 'document_viewer')).toContainText('文档查看员')
  await row(page, 'document_viewer').getByRole('button', { name: '编辑', exact: true }).click()
  dialog = page.getByRole('dialog')
  await expect(dialog.getByLabel('角色代码', { exact: true })).toBeDisabled()
  await dialog.getByLabel('角色名称', { exact: true }).fill('新版文档查看员')
  await dialog.getByRole('button', { name: '确认', exact: true }).click()
  await expect(row(page, 'document_viewer')).toContainText('新版文档查看员')
  await row(page, 'document_viewer').getByRole('button', { name: '停用', exact: true }).click()
  await page.getByRole('dialog').getByRole('button', { name: '确认', exact: true }).click()
  await expect(
    row(page, 'document_viewer').getByRole('button', { name: '启用', exact: true }),
  ).toBeVisible()
  await row(page, 'document_viewer').getByRole('button', { name: '归档', exact: true }).click()
  dialog = page.getByRole('dialog')
  await expect(dialog.getByRole('button', { name: '确认', exact: true })).toBeDisabled()
  await dialog.getByLabel('输入角色代码确认').fill('document_viewer')
  await dialog.getByRole('button', { name: '确认', exact: true }).click()
  await expect(row(page, 'document_viewer')).toHaveCount(0)
  await page.getByLabel('包含已归档角色').check()
  await expect(row(page, 'document_viewer')).toContainText('已归档')
  await expect(row(page, 'document_viewer').getByRole('button')).toHaveCount(0)
  await row(page, 'document_viewer').getByRole('link', { name: '查看关联' }).click()
  await expect(page.locator('.role-permissions-summary')).toContainText('新版文档查看员')
  await expect(page.locator('.role-permissions-summary')).toContainText('已归档')
  await expect(page.locator('.sidebar nav a[href="/roles"]')).toHaveClass(/router-link-active/)
  expect(state.writes.map((request) => request.method)).toEqual([
    'POST',
    'PATCH',
    'PATCH',
    'DELETE',
  ])
  expect(state.writes.slice(1).map((request) => request.version)).toEqual(['1', '2', '3'])
  expect(state.refreshes()).toBe(4)
})

test('permission and scope configuration submits full sets, preserving unknown links and protecting floors', async ({
  page,
}) => {
  const state = await setup(page)
  await openRoles(page)
  await row(page).getByRole('button', { name: '配置权限', exact: true }).click()
  let dialog = page.getByRole('dialog')
  await expect(dialog.getByText(/保留 1 个目录未返回/)).toBeVisible()
  await dialog.getByRole('checkbox', { name: /已停用权限/ }).uncheck()
  await dialog.getByRole('checkbox', { name: /导出报表/ }).check()
  await dialog.getByRole('button', { name: '确认', exact: true }).click()
  await expect(dialog).toHaveCount(0)
  expect(state.assignments.viewer).toEqual(['export', 'system', 'unknown'])
  await row(page).getByRole('button', { name: '管理范围', exact: true }).click()
  dialog = page.getByRole('dialog')
  await dialog.getByRole('checkbox', { name: /范围外角色/ }).check()
  await dialog.getByRole('button', { name: '确认', exact: true }).click()
  await expect(dialog).toHaveCount(0)
  expect(state.scopes.viewer).toEqual(['outside', 'viewer'])
  await row(page, 'super_admin').getByRole('button', { name: '配置权限', exact: true }).click()
  await expect(page.getByRole('dialog').getByRole('checkbox', { name: /业务读取/ })).toBeDisabled()
  await page.getByRole('dialog').getByRole('button', { name: '取消', exact: true }).click()
  await row(page, 'super_admin').getByRole('button', { name: '管理范围', exact: true }).click()
  const checks = page.getByRole('dialog').getByRole('checkbox')
  await expect(checks).toHaveCount(4)
  for (const checkbox of await checks.all()) await expect(checkbox).toBeDisabled()
})

test('read-only, out-of-scope and referenced/system roles cannot expose unsafe mutations', async ({
  page,
}) => {
  await setup(page, ['role.read'])
  await openRoles(page)
  await expect(page.getByRole('button', { name: '新增角色' })).toHaveCount(0)
  await expect(row(page).getByRole('button')).toHaveCount(0)
  await row(page).getByRole('link', { name: '查看关联' }).click()
  await expect(page.locator('.role-permissions-summary')).toContainText('审核员')
  await expect(page.getByText(/仅展示关联权限 ID/)).toBeVisible()
})

test('system floor, out-of-scope and in-use archive buttons are protected', async ({ page }) => {
  await setup(page)
  await openRoles(page)
  await expect(
    row(page, 'super_admin').getByRole('button', { name: '停用', exact: true }),
  ).toHaveCount(0)
  await expect(
    row(page, 'super_admin').getByRole('button', { name: '归档', exact: true }),
  ).toHaveCount(0)
  await expect(row(page, 'outside').getByRole('button')).toHaveCount(0)
  await expect(row(page, 'used').getByRole('button', { name: '归档', exact: true })).toBeDisabled()
})

test('version conflicts require explicit reload and unsaved route changes require confirmation', async ({
  page,
}) => {
  const state = await setup(page)
  await openRoles(page)
  await row(page).getByRole('button', { name: '编辑', exact: true }).click()
  const dialog = page.getByRole('dialog')
  await dialog.getByLabel('角色名称', { exact: true }).fill('冲突修改')
  state.conflict()
  await dialog.getByRole('button', { name: '确认', exact: true }).click()
  await expect(dialog.getByRole('alert').last()).toContainText('2209')
  await expect(dialog.getByRole('button', { name: '确认', exact: true })).toBeDisabled()
  await dialog.getByRole('button', { name: '重新加载', exact: true }).click()
  await expect(dialog.getByLabel('角色名称', { exact: true })).toHaveValue('审核员')
  await dialog.getByLabel('角色名称', { exact: true }).fill('未保存修改')
  await page.evaluate(() => {
    history.pushState({}, '', '/home')
    window.dispatchEvent(new PopStateEvent('popstate'))
  })
  await expect(dialog.getByText('存在未保存修改，确定放弃并离开？')).toBeVisible()
  await dialog.getByRole('button', { name: '继续编辑', exact: true }).click()
  await expect(page).toHaveURL('/roles')
  await dialog.getByRole('button', { name: '取消', exact: true }).click()
  await dialog.getByRole('button', { name: '放弃修改', exact: true }).click()
  await expect(dialog).toHaveCount(0)
})

test('custom-role users can navigate by capability and create users with dynamic manageable roles', async ({
  page,
}) => {
  const state = await setup(
    page,
    ['user.read', 'user.create', 'user.role.assign'],
    'custom_manager',
  )
  await expect(page.locator('.topbar .account')).toContainText('custom_manager')
  await page.locator('.sidebar nav a[href="/users"]').click()
  await expect(page).toHaveURL('/users')
  await page.getByRole('button', { name: '创建用户' }).click()
  const dialog = page.getByRole('dialog')
  await dialog.getByLabel('登录标识', { exact: true }).fill('new_reviewer')
  await dialog.getByLabel('展示名', { exact: true }).fill('新审核员')
  await dialog.getByRole('combobox', { name: '角色', exact: true }).click()
  await expect(page.getByRole('option', { name: 'inactive_reviewer', exact: true })).toHaveCount(0)
  await page.getByRole('option', { name: 'reviewer', exact: true }).click()
  await dialog.getByRole('button', { name: '确认', exact: true }).click()
  await expect(dialog).toHaveCount(0)
  expect(state.writes[0]?.data).toMatchObject({
    role: 'reviewer',
    login_identifier: 'new_reviewer',
  })
})

test('automatic session refresh reloads capabilities after another actor revokes role writes', async ({
  page,
}) => {
  const state = await setup(page)
  await openRoles(page)
  await expect(page.getByRole('button', { name: '新增角色' })).toBeVisible()
  state.revokeWrites()
  await page.getByRole('button', { name: '刷新', exact: true }).click()
  await expect(page.getByRole('button', { name: '新增角色' })).toHaveCount(0)
  await expect(row(page).getByRole('button')).toHaveCount(0)
  expect(state.refreshes()).toBe(1)
  expect(state.writes).toEqual([])
})

test('mobile role table scrolls internally and role metadata failures block writes', async ({
  page,
}) => {
  await page.setViewportSize({ width: 390, height: 844 })
  await setup(page)
  await openRoles(page)
  const table = page.locator('.roles-page .table-scroll')
  await table.hover()
  await page.mouse.wheel(0, 600)
  await expect.poll(() => table.evaluate((element) => element.scrollTop)).toBeGreaterThan(0)
  expect(await page.locator('.main').evaluate((element) => element.scrollTop)).toBe(0)
  await table.evaluate((element) => {
    element.scrollTop = 0
  })
  await page.route('**/api/v1/admin/roles/viewer', (route) =>
    route.fulfill({ status: 503, json: { code: 2216 } }),
  )
  await row(page).getByRole('button', { name: '编辑', exact: true }).click()
  await expect(page.getByRole('dialog').getByRole('alert')).toContainText('2216')
  await expect(
    page.getByRole('dialog').getByRole('button', { name: '确认', exact: true }),
  ).toBeDisabled()
})

test('role editor uses the common Escape close path without bypassing unsaved-change protection', async ({
  page,
}) => {
  await setup(page)
  await openRoles(page)
  const opener = row(page).getByRole('button', { name: '编辑', exact: true })
  await opener.click()
  const dialog = page.getByRole('dialog')
  await expect(dialog.getByLabel('角色名称', { exact: true })).toHaveValue('审核员')
  await page.keyboard.press('Escape')
  await expect(dialog).toHaveCount(0)
  await expect(opener).toBeFocused()
  await opener.click()
  await dialog.getByLabel('角色名称', { exact: true }).fill('未保存的角色名')
  await page.keyboard.press('Escape')
  await expect(dialog.getByText('存在未保存修改，确定放弃并离开？')).toBeVisible()
  await dialog.getByRole('button', { name: '继续编辑', exact: true }).click()
  await expect(dialog.getByLabel('角色名称', { exact: true })).toHaveValue('未保存的角色名')
  await page.keyboard.press('Escape')
  await dialog.getByRole('button', { name: '放弃修改', exact: true }).click()
  await expect(dialog).toHaveCount(0)
})

for (const width of [1440, 390]) {
  test(`role actions scroll behind the header at ${width}px`, async ({ page }) => {
    await page.setViewportSize({ width, height: 844 })
    await setup(page, rights, 'super_admin', 20)
    await page.locator('.sidebar nav a[href="/roles"]').click()
    const table = page.locator('.roles-page .table-scroll')
    await expect(table.locator('tbody tr')).toHaveCount(24)
    for (const theme of ['light', 'dark']) {
      await page.evaluate((theme) => {
        document.documentElement.dataset.theme = theme
      }, theme)
      await expectHeaderAboveRowControls(table)
    }
  })
}

test('page panels share the login theme material in light/dark and lite/full modes', async ({
  page,
}) => {
  await setup(page)
  await openRoles(page)
  await expectThemedPanels(page, '.table-panel')
})
