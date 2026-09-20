import { computed, onMounted, onUnmounted, reactive, watch } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { useAuthStore } from '../stores/auth'
import { isAdmin, passwordPolicy, passwordProblem, roleLabels } from '../domain'
import { useTask } from './useTask'

export function useLoginViewModel() {
  const auth = useAuthStore()
  const router = useRouter()
  const form = reactive({ identifier: '', password: '' })
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
  const links = computed(() =>
    auth.needsPassword
      ? []
      : [
          { to: '/home', label: '工作台', symbol: '⌂' },
          ...(isAdmin(auth.user?.role)
            ? [
                { to: '/users', label: '用户管理', symbol: '◎' },
                { to: '/logs', label: '审计日志', symbol: '≡' },
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
    title: computed(() => String(route.meta.title ?? 'Admin')),
    role: computed(() => (auth.user ? roleLabels[auth.user.role] : '')),
    passwordLink: computed(() => !auth.needsPassword),
  }
}

export function useSessionNavigation() {
  const auth = useAuthStore()
  const router = useRouter()
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
  const load = () => task.run(auth.loadPermissions)
  const capabilityLabels: Record<string, string> = {
    'business.read': '查看业务数据',
    'business.operate': '执行业务操作',
    'engineering.configure': '工程配置',
    'user.read': '查看用户',
    'user.create': '创建用户',
    'user.update': '编辑用户',
    'user.status': '启用与禁用用户',
    'user.unlock': '解锁用户',
    'user.password.reset': '重置用户密码',
    'audit.read': '查看审计日志',
    'user.role.assign': '分配角色',
  }
  onMounted(load)
  return {
    ...task,
    load,
    user: computed(() => auth.user),
    role: computed(() => (auth.user ? roleLabels[auth.user.role] : '')),
    canManage: computed(() => auth.can('user.read')),
    canAudit: computed(() => auth.can('audit.read')),
    capabilities: computed(() =>
      (auth.permissions?.capabilities ?? []).map((code) => ({
        code,
        label: capabilityLabels[code] ?? code,
      })),
    ),
  }
}
