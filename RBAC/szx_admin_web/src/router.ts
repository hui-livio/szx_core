import { createRouter, createWebHistory } from 'vue-router'
import { useAuthStore } from './stores/auth'
import { isAdmin } from './domain'
export const router = createRouter({
  history: createWebHistory(import.meta.env.BASE_URL),
  routes: [
    { path: '/', redirect: '/home' },
    { path: '/login', component: () => import('./pages/LoginPage.vue'), meta: { title: '登录' } },
    {
      path: '/change-password',
      component: () => import('./pages/PasswordPage.vue'),
      meta: { title: '修改密码' },
    },
    { path: '/home', component: () => import('./pages/HomePage.vue'), meta: { title: '工作台' } },
    {
      path: '/users',
      component: () => import('./pages/UsersPage.vue'),
      meta: { title: '用户管理', admin: true },
    },
    {
      path: '/logs',
      component: () => import('./pages/LogsPage.vue'),
      meta: { title: '审计日志', admin: true },
    },
    {
      path: '/:pathMatch(.*)*',
      component: () => import('./pages/NotFoundPage.vue'),
      meta: { title: '页面不存在' },
    },
  ],
})
router.beforeEach((to) => {
  const auth = useAuthStore()
  if (!auth.user) return to.path === '/login' ? true : '/login'
  if (auth.needsPassword && to.path !== '/change-password') return '/change-password'
  if (to.path === '/login') return auth.home
  if (to.meta.admin && !isAdmin(auth.user.role)) return '/home'
  return true
})
router.afterEach((to) => {
  document.title = `${String(to.meta.title ?? 'Admin')} · 数自星智能用户管理系统`
})
