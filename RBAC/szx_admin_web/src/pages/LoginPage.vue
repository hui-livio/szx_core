<script setup lang="ts">
import LoginForm from '../components/auth/LoginForm.vue'
import { useLoginViewModel } from '../composables/useAuthViewModel'
const { form, busy, error, notice, submit } = useLoginViewModel()
</script>
<template>
  <main class="login-page">
    <section class="login-story">
      <div class="brand">
        <span class="brand-mark">S</span>SZX <span class="brand-light">ADMIN</span>
      </div>
      <div class="login-copy">
        <p class="eyebrow">IDENTITY & ACCESS</p>
        <h1>每个身份，<br />各有所属。</h1>
        <p>在同一个控制台中管理用户、分配角色，<br />让每一次访问和操作清晰可查。</p>
        <div class="role-path">
          <span>操作员</span><i /><span>工程师</span><i /><span>管理员</span>
        </div>
      </div>
      <small>SZX · 身份与访问管理</small>
    </section>
    <section class="login-panel">
      <div class="login-card">
        <p class="eyebrow">SZX 管理控制台</p>
        <h2>欢迎登录</h2>
        <p class="subtitle">使用你的账号继续</p>
        <LoginForm v-model="form" :busy="busy" :error="error" :notice="notice" @submit="submit" />
        <p class="login-help">无法登录？请联系管理员检查账号状态。</p>
      </div>
      <small>仅限已授权用户访问</small>
    </section>
  </main>
</template>
