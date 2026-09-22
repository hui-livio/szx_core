<script setup lang="ts">
import { useShellViewModel } from '../composables/useAuthViewModel'
const { links, logout, user, title, role, passwordLink, busy, error } = useShellViewModel()
</script>
<template>
  <div class="app-shell">
    <a class="skip-link" href="#main">跳到主要内容</a>
    <aside class="sidebar">
      <div class="brand">
        <span class="brand-mark">S</span>
        <div>SZX <span class="brand-light">ADMIN</span><small>身份与访问管理</small></div>
      </div>
      <div class="nav-label">管理控制台</div>
      <nav aria-label="主导航">
        <RouterLink v-for="link in links" :key="link.to" :to="link.to"
          ><span aria-hidden="true">{{ link.symbol }}</span
          >{{ link.label }}</RouterLink
        >
      </nav>
      <div class="sidebar-bottom"><span class="system-mark" /> SZX · 权限管理中心</div>
    </aside>
    <div class="workspace">
      <header class="topbar">
        <span class="breadcrumb"
          >控制台 <span>/</span> <strong>{{ title }}</strong></span
        >
        <div class="account">
          <span class="avatar">{{ user?.display_name.slice(0, 1) }}</span>
          <div>
            {{ user?.display_name }}<small>{{ role }}</small>
          </div>
          <RouterLink v-if="passwordLink" to="/change-password">修改密码</RouterLink
          ><button class="text-button" :disabled="busy" @click="logout">
            {{ busy ? '退出中…' : '退出' }}
          </button>
        </div>
      </header>
      <main id="main" class="main">
        <p v-if="error" class="alert error" role="alert">{{ error }}</p>
        <slot />
      </main>
      <footer class="app-footer">SZX Admin <span>统一身份 · 清晰权限 · 操作可追溯</span></footer>
    </div>
  </div>
</template>
