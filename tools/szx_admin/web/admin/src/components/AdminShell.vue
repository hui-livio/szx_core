<script setup lang="ts">
import { computed, ref, useId, watch } from 'vue'
import { LuminaMark, LuminaPopupMenu as Menu } from '../ui/lumina'
import { useShellViewModel } from '../composables/useAuthViewModel'
import { useI18n } from 'vue-i18n'
import { useRoute, useRouter } from 'vue-router'
import UiPreferences from './UiPreferences.vue'
const { links, logout, user, role, passwordLink, busy, error } = useShellViewModel()
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
      </div>
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
      <div class="sidebar-foot">
        <div class="topbar">
          <UiPreferences />
          <div class="account">
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
            <div class="account-identity">
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
        </div>
      </div>
    </aside>
    <div class="workspace">
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
