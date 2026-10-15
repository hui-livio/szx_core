const pages = [
  { to: '/home', label: 'shell.dashboard', symbol: '⌂' },
  {
    to: '/users',
    label: 'shell.users',
    homeLabel: 'home.enterUsers',
    symbol: '◎',
    capability: 'user.read',
  },
  {
    to: '/roles',
    label: 'roleManagement.title',
    symbol: '▦',
    capability: 'role.read',
  },
  { to: '/permissions', label: 'rbac.title', symbol: '◇', capability: 'permission.read' },
  {
    to: '/logs',
    label: 'shell.auditLogs',
    homeLabel: 'home.viewAudit',
    symbol: '≡',
    capabilityAny: ['audit.read', 'behavior.read'],
  },
  {
    to: '/system/performance',
    label: 'performance.title',
    symbol: '◴',
    capability: 'system.metrics.read',
  },
] as const

export function visiblePages(can: (capability: string) => boolean) {
  return pages.filter((page) => hasPageAccess(page, can))
}

export function hasPageAccess(meta: Record<string, unknown>, can: (capability: string) => boolean) {
  return (
    (typeof meta.capability !== 'string' || can(meta.capability)) &&
    (!Array.isArray(meta.capabilityAny) ||
      meta.capabilityAny.some((capability) => typeof capability === 'string' && can(capability)))
  )
}
