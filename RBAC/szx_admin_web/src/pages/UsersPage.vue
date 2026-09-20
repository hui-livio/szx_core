<script setup lang="ts">
import { useUsersViewModel } from '../composables/useUsersViewModel'
import UserFilters from '../components/users/UserFilters.vue'
import UserTable from '../components/users/UserTable.vue'
import UserEditor from '../components/users/UserEditor.vue'
import CredentialNotice from '../components/users/CredentialNotice.vue'
import ModalDialog from '../components/ModalDialog.vue'
import PaginationBar from '../components/PaginationBar.vue'
const {
  filters,
  query,
  mutation,
  action,
  selected,
  credential,
  credentialRequestId,
  success,
  conflict,
  draft,
  roles,
  canAssignRole,
  dialogTitle,
  targetStatus,
  search,
  open,
  close,
  reloadSelected,
  closeCredential,
  submit,
} = useUsersViewModel()
const { items, page, pageCount, total, busy, error, requestId, load } = query
const { busy: saving, error: saveError } = mutation
</script>
<template>
  <header class="page-heading">
    <div>
      <p class="eyebrow">IDENTITY DIRECTORY</p>
      <h1>用户管理</h1>
      <p>管理账号、角色与访问状态。</p>
    </div>
    <button class="primary" @click="open('create')">＋ 创建用户</button>
  </header>
  <p v-if="success" class="alert success" role="status">{{ success }}</p>
  <section class="panel table-panel">
    <UserFilters v-model="filters" :roles="roles" :busy="busy" @search="search" />
    <div class="section-heading table-heading">
      <h2>
        用户目录 <span class="count">{{ total }}</span>
      </h2>
      <button :disabled="busy" @click="load()">刷新列表</button>
    </div>
    <p v-if="error" class="alert error" role="alert">{{ error }}</p>
    <UserTable
      :users="items"
      :busy="busy"
      :failed="!!error"
      :can-assign-role="canAssignRole"
      @action="open"
    />
    <PaginationBar
      :page="page"
      :page-count="pageCount"
      :total="total"
      :busy="busy"
      @change="load"
    />
  </section>
  <p v-if="requestId" class="request-id">请求 ID：{{ requestId }}</p>
  <ModalDialog v-if="action" :title="dialogTitle" :busy="saving" @close="close">
    <UserEditor
      v-model="draft"
      :action="action"
      :user="selected"
      :roles="roles"
      :busy="saving"
      :error="saveError"
      :conflict="conflict"
      :target-status="targetStatus"
      @submit="submit"
      @close="close"
      @reload="reloadSelected"
    />
  </ModalDialog>
  <ModalDialog v-if="credential" title="临时密码已生成" @close="closeCredential">
    <CredentialNotice
      :issue="credential"
      :request-id="credentialRequestId"
      @close="closeCredential"
    />
  </ModalDialog>
</template>
