import { createRouter, createWebHistory } from 'vue-router'
import { watch } from 'vue'
import { useAuthStore } from './stores/auth'
import { isAdmin } from './domain'
import { i18n } from './i18n'
export const router = createRouter({
  history: createWebHistory(import.meta.env.BASE_URL),
  routes: [
    { path: '/', redirect: '/home' },
    {
      path: '/login',
      component: () => import('./pages/LoginPage.vue'),
      meta: { titleKey: 'login.welcome' },
    },
    {
      path: '/change-password',
      component: () => import('./pages/PasswordPage.vue'),
      meta: { titleKey: 'password.title' },
    },
    {
      path: '/home',
      component: () => import('./pages/HomePage.vue'),
      meta: { titleKey: 'shell.dashboard' },
    },
    {
      path: '/users',
      component: () => import('./pages/UsersPage.vue'),
      meta: { titleKey: 'shell.users', admin: true },
    },
    {
      path: '/logs',
      component: () => import('./pages/LogsPage.vue'),
      meta: { titleKey: 'shell.auditLogs', admin: true },
    },
    {
      path: '/roles',
      component: () => import('./pages/RolePermissionsPage.vue'),
      meta: { titleKey: 'rolePermissions.title', capability: 'role.read' },
    },
    {
      path: '/permissions',
      component: () => import('./pages/PermissionsPage.vue'),
      meta: { titleKey: 'rbac.title', capability: 'permission.read' },
    },
    {
      path: '/system/performance',
      component: () => import('./pages/PerformancePage.vue'),
      meta: { titleKey: 'performance.title', admin: true, capability: 'system.metrics.read' },
    },
    {
      path: '/:pathMatch(.*)*',
      component: () => import('./pages/NotFoundPage.vue'),
      meta: { titleKey: 'notFound.title' },
    },
  ],
})
router.beforeEach(async (to) => {
  const auth = useAuthStore()
  if (!auth.user) return to.path === '/login' ? true : '/login'
  if (auth.needsPassword && to.path !== '/change-password') return '/change-password'
  if (to.path === '/login') return auth.home
  if (to.meta.admin && !isAdmin(auth.user.role)) return '/home'
  if (typeof to.meta.capability === 'string') {
    if (!auth.permissions) {
      try {
        await auth.loadPermissions()
      } catch {
        return auth.user ? '/home' : '/login'
      }
    }
    if (!auth.user) return '/login'
    if (auth.needsPassword) return '/change-password'
    if (!auth.can(to.meta.capability)) return '/home'
  }
  return true
})
router.afterEach((to) => {
  const titleKey = typeof to.meta.titleKey === 'string' ? to.meta.titleKey : 'shell.adminConsole'
  document.title = `${i18n.global.t(titleKey)} · SZX Admin`
})

watch(i18n.global.locale, () => {
  const route = router.currentRoute.value
  const titleKey =
    typeof route.meta.titleKey === 'string' ? route.meta.titleKey : 'shell.adminConsole'
  document.title = `${i18n.global.t(titleKey)} · SZX Admin`
})
