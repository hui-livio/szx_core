import { computed, onMounted, onUnmounted, reactive, watch } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { useAuthStore } from '../stores/auth'
import { isAdmin, passwordPolicy, passwordProblem, roleLabels } from '../domain'
import { useTask } from './useTask'
import { useI18n } from 'vue-i18n'

export function useLoginViewModel() {
  const auth = useAuthStore()
  const router = useRouter()
  const form = reactive({
    identifier: '',
    password: '',
  })
  const task = useTask()
  const submit = () =>
    task.run(async () => {
      try {
        await auth.login(form.identifier, form.password)
        await router.replace(auth.needsPassword ? '/change-password' : auth.home)
      } finally {
        form.password = ''
      }
    })
  onUnmounted(() => {
    form.password = ''
  })
  return { form, notice: computed(() => auth.notice), ...task, submit }
}

export function usePasswordViewModel() {
  const auth = useAuthStore()
  const router = useRouter()
  const form = reactive({ oldPassword: '', password: '', confirmation: '' })
  const task = useTask()
  const forced = computed(() => Boolean(auth.user?.force_password_change))
  const recommended = computed(() => Boolean(auth.user?.password_change_recommended))
  const policy = computed(() => passwordPolicy(auth.user?.role))
  function clear() {
    form.oldPassword = ''
    form.password = ''
    form.confirmation = ''
  }
  const submit = () =>
    task.run(async () => {
      const problem = passwordProblem(
        form.password,
        form.oldPassword,
        form.confirmation,
        auth.user?.role,
      )
      if (problem) throw new Error(problem)
      try {
        await auth.changePassword(form.oldPassword, form.password)
        await router.replace('/login')
      } finally {
        clear()
      }
    })
  async function skip() {
    if (forced.value || task.busy.value) return
    auth.recommendationSkipped = true
    await router.replace(auth.home)
  }
  onUnmounted(clear)
  return { form, forced, recommended, policy, ...task, submit, skip }
}

export function useShellViewModel() {
  const auth = useAuthStore()
  const router = useRouter()
  const route = useRoute()
  const task = useTask()
  const { t } = useI18n()
  const links = computed(() =>
    auth.needsPassword
      ? []
      : [
          { to: '/home', label: t('shell.dashboard'), symbol: '⌂' },
          ...(auth.can('role.read')
            ? [{ to: '/roles', label: t('rolePermissions.title'), symbol: '▦' }]
            : []),
          ...(auth.can('permission.read')
            ? [{ to: '/permissions', label: t('rbac.title'), symbol: '◇' }]
            : []),
          ...(auth.can('system.metrics.read')
            ? [{ to: '/system/performance', label: t('performance.title'), symbol: '◴' }]
            : []),
          ...(isAdmin(auth.user?.role)
            ? [
                { to: '/users', label: t('shell.users'), symbol: '◎' },
                { to: '/logs', label: t('shell.auditLogs'), symbol: '≡' },
              ]
            : []),
        ],
  )
  const logout = () =>
    task.run(async () => {
      await auth.logout()
      await router.replace('/login')
    })
  return {
    ...task,
    links,
    logout,
    user: computed(() => auth.user),
    title: computed(() => (route.meta.titleKey ? t(String(route.meta.titleKey)) : 'Admin')),
    role: computed(() => (auth.user ? t(roleLabels[auth.user.role]) : '')),
    passwordLink: computed(() => !auth.needsPassword),
  }
}

export function useSessionNavigation() {
  const auth = useAuthStore()
  const router = useRouter()
  watch(
    () => [auth.user?.id, auth.user?.authz_version] as const,
    () => {
      if (auth.user && !auth.permissions) void auth.loadPermissions().catch(() => {})
    },
    { immediate: true },
  )
  watch(
    () => auth.user,
    (user) => {
      if (!user) void router.replace('/login')
      else if (user.force_password_change) void router.replace('/change-password')
      else if (router.currentRoute.value.meta.admin && !isAdmin(user.role))
        void router.replace('/home')
    },
  )
  return { authenticated: computed(() => Boolean(auth.user)) }
}

export function useHomeViewModel() {
  const auth = useAuthStore()
  const task = useTask()
  const { t } = useI18n()
  const load = () => task.run(auth.loadPermissions)
  const capabilityLabels: Record<string, string> = {
    'system.metrics.read': 'performance.permission',
    'business.read': 'permissions.business_read',
    'business.operate': 'permissions.business_operate',
    'engineering.configure': 'permissions.engineering_configure',
    'user.read': 'permissions.user_read',
    'user.create': 'permissions.user_create',
    'user.update': 'permissions.user_update',
    'user.status': 'permissions.user_status',
    'user.unlock': 'permissions.user_unlock',
    'user.password.reset': 'permissions.user_password_reset',
    'audit.read': 'permissions.audit_read',
    'user.role.assign': 'permissions.user_role_assign',
    'behavior.read': 'permissions.behavior_read',
    'role.read': 'permissions.role_read',
    'role.create': 'permissions.role_create',
    'role.update': 'permissions.role_update',
    'role.status': 'permissions.role_status',
    'role.archive': 'permissions.role_archive',
    'role.permission.assign': 'permissions.role_permission_assign',
    'role.scope.assign': 'permissions.role_scope_assign',
    'permission.read': 'permissions.permission_read',
    'permission.create': 'permissions.permission_create',
    'permission.update': 'permissions.permission_update',
    'permission.status': 'permissions.permission_status',
    'permission.archive': 'permissions.permission_archive',
  }
  onMounted(load)
  return {
    ...task,
    load,
    user: computed(() => auth.user),
    role: computed(() => (auth.user ? t(roleLabels[auth.user.role]) : '')),
    canViewRoles: computed(() => auth.can('role.read')),
    canManage: computed(() => auth.can('user.read')),
    canAudit: computed(() => auth.can('audit.read')),
    capabilities: computed(() =>
      (auth.permissions?.capabilities ?? []).map((code) => ({
        code,
        label: capabilityLabels[code] ? t(capabilityLabels[code]) : code,
      })),
    ),
  }
}
