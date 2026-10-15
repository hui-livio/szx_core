<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, useId, watch } from 'vue'
import { LuminaMark, LuminaPopupMenu as Menu } from '../ui/lumina'
import { useShellViewModel } from '../composables/useAuthViewModel'
import { useI18n } from 'vue-i18n'
import { useRoute, useRouter } from 'vue-router'
import UiPreferences from './UiPreferences.vue'
const { links, logout, user, title, role, passwordLink, busy, error } = useShellViewModel()
const { t } = useI18n()
const route = useRoute()
const router = useRouter()
const accountMenu = ref<InstanceType<typeof Menu>>()
const accountMenuOpen = ref(false)
const accountMenuId = `${useId()}-account-menu`
const accountActions = computed(() => [
  {
    label: t('shell.password'),
    visible: passwordLink.value,
    disabled: busy.value,
    command: () => void router.push('/change-password'),
  },
  {
    label: busy.value ? t('shell.logoutBusy') : t('shell.logout'),
    disabled: busy.value,
    command: () => void logout(),
  },
])

watch(
  () => route.fullPath,
  () => accountMenu.value?.hide(),
)
const narrowScreen = ref(false)
let narrowScreenQuery: MediaQueryList | undefined

function updateNarrowScreen() {
  accountMenu.value?.hide()
  narrowScreen.value = narrowScreenQuery?.matches ?? false
}

onMounted(() => {
  narrowScreenQuery = window.matchMedia('(max-width: 720px)')
  updateNarrowScreen()
  narrowScreenQuery.addEventListener('change', updateNarrowScreen)
})

onBeforeUnmount(() => narrowScreenQuery?.removeEventListener('change', updateNarrowScreen))
</script>
<template>
  <div class="app-shell" :class="{ 'data-shell': route.meta.tableLayout }">
    <aside class="sidebar">
      <div class="sidebar-header">
        <div class="brand">
          <LuminaMark class="brand-mark" />
          <div>
            SZX <span class="brand-light">ADMIN</span
            ><small>{{ t('shell.identityManagement') }}</small>
          </div>
        </div>
        <div id="mobile-topbar" />
      </div>
      <div class="nav-label">{{ t('shell.adminConsole') }}</div>
      <nav :aria-label="t('shell.mainNavigation')">
        <RouterLink
          class="lumina-smooth-edge"
          :class="{ 'router-link-active': route.path.startsWith(link.to + '/') }"
          v-for="link in links"
          :key="link.to"
          :to="link.to"
          ><span aria-hidden="true">{{ link.symbol }}</span
          >{{ link.label }}</RouterLink
        >
      </nav>
      <div class="sidebar-bottom">
        <span class="system-mark" /> SZX · {{ t('shell.identityManagement') }}
      </div>
    </aside>
    <div class="workspace">
      <Teleport to="#mobile-topbar" :disabled="!narrowScreen" defer>
        <header class="topbar">
          <span class="breadcrumb"
            >{{ t('shell.console') }} <span>/</span> <strong>{{ title }}</strong></span
          >
          <div class="account">
            <UiPreferences />
            <LuminaMark
              class="avatar"
              interactive
              :disabled="busy"
              :label="t('shell.accountMenu')"
              :title="t('shell.accountMenu')"
              aria-haspopup="menu"
              :aria-controls="accountMenuId"
              :aria-expanded="accountMenuOpen"
              @click="accountMenu?.toggle($event)"
              :text="user?.display_name.slice(0, 1)"
              variant="brand"
              radius="50%"
              size="var(--lg-h-icon)"
              font-size="var(--lg-fs-xl)"
              :font-weight="500"
            />
            <div>
              {{ user?.display_name }}<small>{{ role }}</small>
            </div>
            <Menu
              ref="accountMenu"
              :id="accountMenuId"
              class="account-menu"
              :model="accountActions"
              :aria-label="t('shell.accountMenu')"
              @show="accountMenuOpen = true"
              @hide="accountMenuOpen = false"
            />
          </div>
        </header>
      </Teleport>
      <main id="main" class="main lumina-scrollbar">
        <p v-if="error" class="alert error" role="alert">{{ error }}</p>
        <slot />
      </main>
      <footer class="app-footer">
        SZX Admin <span>{{ t('shell.footer') }}</span>
      </footer>
    </div>
  </div>
</template>
