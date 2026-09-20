<script setup lang="ts">
import { useHomeViewModel } from '../composables/useAuthViewModel'
import { formatTime } from '../domain'
const { user, role, capabilities, canManage, canAudit, busy, error, load } = useHomeViewModel()
</script>
<template>
  <header class="page-heading">
    <div>
      <p class="eyebrow">WORKSPACE</p>
      <h1>你好，{{ user?.display_name }}</h1>
      <p>查看当前身份与可用权限。</p>
    </div>
    <span class="role-badge">{{ role }}</span>
  </header>
  <section class="panel">
    <h2>账号概览</h2>
    <dl class="details-grid">
      <div>
        <dt>登录账号</dt>
        <dd>{{ user?.uid ?? user?.username }}</dd>
      </div>
      <div>
        <dt>最近登录</dt>
        <dd>{{ formatTime(user?.last_login_at ?? null) }}</dd>
      </div>
    </dl>
    <div class="actions">
      <RouterLink v-if="canManage" class="button primary" to="/users">进入用户管理 →</RouterLink>
      <RouterLink v-if="canAudit" class="button" to="/logs">查看审计日志</RouterLink>
      <RouterLink class="button" to="/change-password">修改密码</RouterLink>
    </div>
  </section>
  <section class="panel">
    <div class="section-heading">
      <h2>我的权限</h2>
      <button :disabled="busy" @click="load">{{ busy ? '加载中…' : '刷新权限' }}</button>
    </div>
    <p v-if="error" class="alert error" role="alert">{{ error }}</p>
    <div class="capability-list">
      <span v-for="item in capabilities" :key="item.code">✓ {{ item.label }}</span>
    </div>
    <p class="hint">业务操作请在对应业务客户端中使用。</p>
  </section>
</template>
