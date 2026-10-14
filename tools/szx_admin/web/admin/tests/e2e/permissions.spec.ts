import { test, expect, type Page } from '@playwright/test'
import type { PermissionRecord, RoleRecord } from '../../src/api/types'

const builtIn: PermissionRecord = {
  id: 'system-permission',
  code: 'user.read',
  name: 'Read users',
  description: '',
  module: 'user',
  enforcement: 'server',
  source: 'system',
  status: 'active',
  required_for_super_admin: true,
  version: 1,
}
const custom: PermissionRecord = {
  id: 'custom-permission',
  code: 'report.view',
  name: '查看报表',
  description: '报表说明',
  module: 'reports',
  enforcement: 'client',
  source: 'custom',
  status: 'active',
  required_for_super_admin: false,
  version: 1,
}
async function setup(page: Page, allow = true, readOnly = false, disabledBinding = false) {
  let items: PermissionRecord[] = [
    { ...builtIn },
    { ...custom, status: disabledBinding ? 'disabled' : 'active' },
  ]
  if (disabledBinding)
    items.push({
      ...custom,
      id: 'unbound-disabled',
      code: 'report.other',
      name: '未关联停用权限',
      status: 'disabled',
    })
  let conflict = false
  let assigned = disabledBinding ? [builtIn.id, custom.id] : [builtIn.id]
  const writes: { method: string; path: string; data: unknown; version?: string }[] = []
  const role: RoleRecord = {
    id: 'role-operator',
    code: 'operator',
    name: '操作员',
    description: '',
    system_key: 'operator',
    status: 'active',
    version: 3,
    authz_version: 3,
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
    const method = request.method()
    const data = request.postData() ? request.postDataJSON() : undefined
    const reply = (data: unknown, status = 200) =>
      route.fulfill({ status, json: { data, request_id: 'permissions-test' } })
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
    if (path === '/client/me/permissions')
      return reply({
        user_id: user.id,
        role: user.role,
        authz_version: 1,
        capabilities: !allow
          ? []
          : readOnly
            ? ['permission.read']
            : [
                'permission.read',
                'permission.create',
                'permission.update',
                'permission.status',
                'permission.archive',
                'role.read',
                'role.permission.assign',
              ],
      })
    if (method !== 'GET')
      writes.push({ method, path, data, version: request.headers()['if-match'] })
    if (path === '/admin/permissions' && method === 'GET') {
      const archived = new URL(request.url()).searchParams.get('include_archived') === 'true'
      return reply({ items: items.filter((item) => archived || item.status !== 'archived') })
    }
    if (path === '/admin/permissions' && method === 'POST') {
      const created = { ...custom, ...data, id: 'created', version: 1 }
      items.push(created)
      return reply(created, 201)
    }
    if (path === '/admin/roles') return reply({ items: [role] })
    if (path === '/admin/roles/role-operator') return reply(role)
    if (path.endsWith('/roles/role-operator/permissions')) {
      if (method === 'GET') return reply({ permission_ids: assigned })
      assigned = data.permission_ids
      role.version++
      role.authz_version++
      return reply(role)
    }
    const id = path.split('/')[3]
    const item = items.find((value) => value.id === id)
    if (item && method === 'GET') return reply(item)
    if (item && method !== 'GET') {
      if (conflict) {
        conflict = false
        item.version++
        item.name = '并发更新'
        return route.fulfill({ status: 409, json: { code: 2209, request_id: 'stale' } })
      }
      item.version++
      if (method === 'DELETE') {
        item.status = 'archived'
        return route.fulfill({ status: 204 })
      }
      Object.assign(item, data)
      return reply(item)
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
    conflict: () => {
      conflict = true
    },
    assigned: () => assigned,
  }
}

async function navigate(page: Page) {
  await page.getByRole('link', { name: '权限管理', exact: true }).click()
}

test('custom permission create/edit/status/archive with stable metadata and system protection', async ({
  page,
}) => {
  const state = await setup(page)
  await navigate(page)
  const protectedRow = page.getByRole('row').filter({ hasText: 'user.read' })
  await expect(protectedRow).toContainText('受保护')
  await expect(protectedRow.getByRole('button')).toHaveCount(0)
  await page.getByRole('button', { name: '新增权限', exact: false }).click()
  const dialog = page.getByRole('dialog')
  await dialog.getByLabel('权限代码', { exact: true }).fill(' report.export ')
  // Browser pattern validation intentionally rejects surrounding spaces; submit valid wire code.
  await dialog.getByLabel('权限代码', { exact: true }).fill('report.export')
  await dialog.getByLabel('权限名称', { exact: true }).fill('导出报表')
  await dialog.getByLabel('模块', { exact: true }).fill('reports')
  await dialog.getByLabel('说明', { exact: true }).fill('导出说明')
  await dialog.getByRole('button', { name: '确认', exact: true }).click()
  await expect(dialog).toHaveCount(0)
  const created = page.getByRole('row').filter({ hasText: 'report.export' })
  await expect(created).toContainText('导出说明')
  expect(state.writes.at(-1)?.data).toEqual({
    code: 'report.export',
    name: '导出报表',
    module: 'reports',
    description: '导出说明',
  })
  await created.getByRole('button', { name: '编辑', exact: true }).click()
  await expect(dialog.getByLabel('权限代码', { exact: true })).toBeDisabled()
  await dialog.getByLabel('说明', { exact: true }).fill('新版说明')
  await dialog.getByRole('button', { name: '确认', exact: true }).click()
  await expect(dialog).toHaveCount(0)
  expect(state.writes.at(-1)?.version).toBe('1')
  await created.getByRole('button', { name: '停用', exact: true }).click()
  await dialog.getByRole('button', { name: '确认', exact: true }).click()
  await expect(created.getByRole('button', { name: '启用', exact: true })).toBeVisible()
  await created.getByRole('button', { name: '归档', exact: true }).click()
  await dialog.getByRole('button', { name: '确认', exact: true }).click()
  await expect(created).toHaveCount(0)
  await page.getByLabel('包含已归档').check()
  await expect(created).toContainText('已归档')
  await expect(created.getByRole('button')).toHaveCount(0)
})

test('version conflict blocks retry until explicit reload', async ({ page }) => {
  const state = await setup(page)
  await navigate(page)
  await page
    .getByRole('row')
    .filter({ hasText: 'report.view' })
    .getByRole('button', { name: '编辑', exact: true })
    .click()
  const dialog = page.getByRole('dialog')
  await dialog.getByLabel('权限名称', { exact: true }).fill('新的名称')
  state.conflict()
  await dialog.getByRole('button', { name: '确认', exact: true }).click()
  await expect(dialog.getByRole('button', { name: '确认', exact: true })).toBeDisabled()
  await dialog.getByRole('button', { name: '重新加载', exact: true }).click()
  await expect(dialog.getByLabel('权限名称', { exact: true })).toHaveValue('并发更新')
  await dialog.getByRole('button', { name: '确认', exact: true }).click()
  await expect(dialog).toHaveCount(0)
  expect(state.writes.at(-1)?.version).toBe('2')
})

test('assigning custom client permission preserves existing system IDs', async ({ page }) => {
  const state = await setup(page)
  await navigate(page)
  await page.getByRole('button', { name: '分配角色权限', exact: true }).click()
  const dialog = page.getByRole('dialog')
  await dialog.getByRole('combobox', { name: '角色', exact: true }).click()
  await page.getByRole('option', { name: '操作员', exact: true }).click()
  await dialog.getByLabel(/查看报表/).check()
  await dialog.getByRole('button', { name: '确认', exact: true }).click()
  await expect(dialog).toHaveCount(0)
  expect(state.assigned()).toEqual(['custom-permission', 'system-permission'])
  expect(state.writes.at(-1)?.version).toBe('3')
})

test('permissions navigation fails closed without read capability', async ({ page }) => {
  await setup(page, false)
  await expect(page.getByRole('link', { name: '权限管理' })).toHaveCount(0)
  await page.evaluate(() => {
    history.pushState({}, '', '/permissions')
    window.dispatchEvent(new PopStateEvent('popstate'))
  })
  await expect(page).toHaveURL('/home')
})

test('read-only capability hides mutations and keeps dark mobile table usable', async ({
  page,
}) => {
  await page.setViewportSize({ width: 390, height: 844 })
  const errors: string[] = []
  page.on('pageerror', (error) => errors.push(error.message))
  await setup(page, true, true)
  await navigate(page)
  await page.evaluate(() => {
    document.documentElement.dataset.theme = 'dark'
  })
  await expect(page.getByRole('button', { name: '新增权限', exact: false })).toHaveCount(0)
  await expect(page.getByRole('button', { name: '分配角色权限' })).toHaveCount(0)
  await expect(page.getByRole('button', { name: '编辑', exact: true })).toHaveCount(0)
  await expect(page.locator('.table-scroll')).toBeVisible()
  expect(await page.evaluate(() => document.documentElement.scrollWidth)).toBeLessThanOrEqual(390)
  expect(errors).toEqual([])
})

test('disabled custom permission can be unbound and archived without ever re-enabling it', async ({
  page,
}) => {
  const state = await setup(page, true, false, true)
  await navigate(page)
  await page.getByRole('button', { name: '分配角色权限', exact: true }).click()
  const dialog = page.getByRole('dialog')
  await dialog.getByRole('combobox', { name: '角色', exact: true }).click()
  await page.getByRole('option', { name: '操作员', exact: true }).click()
  const binding = dialog.getByRole('checkbox', { name: /查看报表/ })
  await expect(binding).toBeChecked()
  await expect(dialog.getByText('未关联停用权限')).toHaveCount(0)
  await binding.uncheck()
  // Keep the option visible to allow cancelling an accidental removal in the same edit.
  await expect(binding).toBeVisible()
  await dialog.getByRole('button', { name: '确认', exact: true }).click()
  await expect(dialog).toHaveCount(0)
  expect(state.assigned()).toEqual([builtIn.id])
  await page
    .getByRole('row')
    .filter({ hasText: 'report.view' })
    .getByRole('button', { name: '归档', exact: true })
    .click()
  await dialog.getByRole('button', { name: '确认', exact: true }).click()
  await expect(dialog).toHaveCount(0)
  expect(state.writes.filter((write) => write.path.endsWith('/status'))).toEqual([])
  await expect(page.getByRole('row').filter({ hasText: 'report.view' })).toHaveCount(0)
})
