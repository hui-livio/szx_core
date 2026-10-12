<script setup lang="ts">
import { useUsersViewModel } from '../composables/useUsersViewModel'
import UserFilters from '../components/users/UserFilters.vue'
import UserTable from '../components/users/UserTable.vue'
import UserEditor from '../components/users/UserEditor.vue'
import CredentialNotice from '../components/users/CredentialNotice.vue'
import ModalDialog from '../components/ModalDialog.vue'
import PaginationBar from '../components/PaginationBar.vue'
import { useI18n } from 'vue-i18n'
const { t } = useI18n()
const {
  filters,
  users,
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
  passwordHint,
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
const { page, pageCount, total, busy, error, requestId, load } = query
const { busy: saving, error: saveError } = mutation
</script>
<template>
  <div class="data-page users-page">
    <header class="page-heading">
      <div>
        <p class="eyebrow">{{ t('users.eyebrow') }}</p>
        <h1>{{ t('users.title') }}</h1>
        <p>{{ t('users.subtitle') }}</p>
      </div>
      <button class="primary" @click="open('create')">＋ {{ t('users.create') }}</button>
    </header>
    <p v-if="success" class="alert success" role="status">{{ success }}</p>
    <section class="panel table-panel">
      <UserFilters v-model="filters" :roles="roles" :busy="busy" @search="search" />
      <div class="section-heading table-heading">
        <h2>
          {{ t('users.directory') }} <span class="count">{{ total }}</span>
        </h2>
        <button :disabled="busy" @click="load()">{{ t('users.refresh') }}</button>
      </div>
      <p v-if="error" class="alert error" role="alert">{{ error }}</p>
      <UserTable
        :users="users"
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
    <p v-if="requestId" class="request-id">{{ t('common.requestId') }}：{{ requestId }}</p>
    <ModalDialog v-if="action" :title="dialogTitle" :busy="saving" @close="close">
      <UserEditor
        v-model="draft"
        :action="action"
        :user="selected"
        :roles="roles"
        :password-hint="passwordHint"
        :busy="saving"
        :error="saveError"
        :conflict="conflict"
        :target-status="targetStatus"
        @submit="submit"
        @close="close"
        @reload="reloadSelected"
      />
    </ModalDialog>
    <ModalDialog v-if="credential" :title="t('users.credentialTitle')" @close="closeCredential">
      <CredentialNotice
        :issue="credential"
        :request-id="credentialRequestId"
        @close="closeCredential"
      />
    </ModalDialog>
  </div>
</template>
