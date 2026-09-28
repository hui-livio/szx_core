import { computed, shallowRef } from 'vue'
import { acceptHMRUpdate, defineStore } from 'pinia'
import { api } from '../api/client'
import type { Permissions, User } from '../api/types'
import { hashPassword, isAdmin } from '../domain'
import { translate } from '../i18n'

export const useAuthStore = defineStore('auth', () => {
  const user = shallowRef<User | null>(null)
  const permissions = shallowRef<Permissions | null>(null)
  const recommendationSkipped = shallowRef(false)
  const notice = shallowRef('')
  const home = computed(() => (isAdmin(user.value?.role) ? '/users' : '/home'))
  const needsPassword = computed(() =>
    Boolean(
      user.value?.force_password_change ||
      (user.value?.password_change_recommended && !recommendationSkipped.value),
    ),
  )
  api.onSessionChange((session) => {
    if (
      !session ||
      session.user.id !== user.value?.id ||
      session.user.authz_version !== user.value?.authz_version
    ) {
      permissions.value = null
      recommendationSkipped.value = false
    }
    user.value = session?.user ?? null
  })
  async function loadPermissions() {
    const response = await api.request<Permissions>({ url: '/client/me/permissions' })
    permissions.value = response.data
  }
  async function login(identifier: string, password: string) {
    notice.value = ''
    recommendationSkipped.value = false
    await api.login(identifier, await hashPassword(password))
  }
  async function changePassword(oldPassword: string, password: string) {
    await api.refresh()
    if (!user.value) throw new Error(translate('errors.sessionInvalid'))
    const version = user.value.version
    const [oldHash, newHash] = await Promise.all([
      hashPassword(oldPassword),
      hashPassword(password),
    ])
    await api.request({
      url: '/auth/change-password',
      method: 'POST',
      headers: { 'If-Match': String(version) },
      data: { old_password_hash: oldHash, new_password_hash: newHash },
    })
    api.setSession(null)
    notice.value = translate('password.changedNotice')
  }
  function updateSelf(value: User) {
    if (value.id !== user.value?.id) return
    if (value.authz_version !== user.value.authz_version) {
      api.setSession(null)
      notice.value = translate('errors.credentialsChanged')
    } else user.value = value
  }
  return {
    user,
    permissions,
    recommendationSkipped,
    notice,
    home,
    needsPassword,
    login,
    loadPermissions,
    changePassword,
    updateSelf,
    can: (capability: string) => Boolean(permissions.value?.capabilities.includes(capability)),
    logout: () => api.logout(),
  }
})
if (import.meta.hot) import.meta.hot.accept(acceptHMRUpdate(useAuthStore, import.meta.hot))
