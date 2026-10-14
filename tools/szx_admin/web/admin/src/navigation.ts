import type { Role } from './api/types'
import { isAdmin } from './domain'

const pages = [
  { to: '/home', label: 'shell.dashboard', symbol: '⌂' },
  {
    to: '/users',
    label: 'shell.users',
    homeLabel: 'home.enterUsers',
    symbol: '◎',
    homeCapability: 'user.read',
    admin: true,
  },
  {
    to: '/roles',
    label: 'rolePermissions.title',
    homeLabel: 'rolePermissions.viewAssociations',
    symbol: '▦',
    capability: 'role.read',
  },
  { to: '/permissions', label: 'rbac.title', symbol: '◇', capability: 'permission.read' },
  {
    to: '/logs',
    label: 'shell.auditLogs',
    homeLabel: 'home.viewAudit',
    symbol: '≡',
    homeCapability: 'audit.read',
    admin: true,
  },
  {
    to: '/system/performance',
    label: 'performance.title',
    symbol: '◴',
    capability: 'system.metrics.read',
    admin: true,
  },
] as const

export function visiblePages(role: Role | undefined, can: (capability: string) => boolean) {
  return pages.filter(
    (page) =>
      (!('admin' in page) || !page.admin || isAdmin(role)) &&
      (!('capability' in page) || can(page.capability)),
  )
}
