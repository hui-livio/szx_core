<script setup lang="ts">
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'
import {
  LuminaButton as Button,
  LuminaInput as Input,
  LuminaSelect as Select,
  LuminaTable as Table,
  LuminaColumn as Column,
} from '../ui/lumina'
import ModalDialog from '../components/ModalDialog.vue'
import { usePermissions } from '../features/permissions/usePermissions'
import type { PermissionRecord } from '../api/types'
const { t } = useI18n()
const {
  filter,
  visible,
  loading,
  error,
  success,
  action,
  selected,
  selectedRole,
  assigned,
  roleId,
  roles,
  conflict,
  draft,
  mutation,
  can,
  assignmentOptions,
  load,
  open,
  close,
  openAssignment,
  chooseRole,
  reloadSelected,
  submit,
} = usePermissions()
const { busy: saving, error: saveError } = mutation
const title = computed(() =>
  t(
    action.value === 'create'
      ? 'rbac.create'
      : action.value === 'edit'
        ? 'rbac.edit'
        : action.value === 'assign'
          ? 'rbac.assign'
          : action.value === 'archive'
            ? 'rbac.archive'
            : selected.value?.status === 'active'
              ? 'rbac.disable'
              : 'rbac.enable',
  ),
)
const sources = computed(() => [
  { value: '', label: t('rbac.all') },
  { value: 'system', label: t('rbac.system') },
  { value: 'custom', label: t('rbac.custom') },
])
</script>
<template>
  <div class="data-page permissions-page">
    <header class="page-heading">
      <div>
        <h1>{{ t('rbac.title') }}</h1>
        <p>{{ t('rbac.subtitle') }}</p>
      </div>
      <div class="actions">
        <Button
          v-if="can('role.read') && can('role.permission.assign')"
          :disabled="saving || loading"
          @click="openAssignment"
          type="button"
          >{{ t('rbac.assign') }}</Button
        >
        <Button
          v-if="can('permission.create')"
          variant="primary"
          type="button"
          @click="open('create')"
          >＋ {{ t('rbac.create') }}</Button
        >
      </div>
    </header>
    <p v-if="success" class="alert success" role="status">{{ success }}</p>
    <section class="panel table-panel">
      <form class="filters" @submit.prevent="load">
        <label
          >{{ t('rbac.search')
          }}<Input v-model="filter.search" type="search" :placeholder="t('rbac.searchHint')"
        /></label>
        <label
          >{{ t('rbac.source')
          }}<Select
            v-model="filter.source"
            :options="sources"
            option-label="label"
            option-value="value"
            :aria-label="t('rbac.source')"
        /></label>
        <label class="permission-check"
          ><input v-model="filter.includeArchived" type="checkbox" />{{
            t('rbac.includeArchived')
          }}</label
        >
        <Button type="submit" :disabled="loading">{{ t('common.refresh') }}</Button>
      </form>
      <p class="hint">{{ t('rbac.clientHint') }}</p>
      <p v-if="error" class="alert error" role="alert">{{ error }}</p>
      <Table
        :value="visible"
        data-key="id"
        class="admin-table"
        min-width="900px"
        :pt="{ tableContainer: { class: 'table-scroll', 'aria-busy': loading } }"
      >
        <Column :header="t('rbac.name')"
          ><template #body="{ data }: { data: PermissionRecord }"
            ><strong>{{ data.name }}</strong
            ><small class="identity mono">{{ data.code }}</small
            ><small v-if="data.description" class="identity">{{
              data.description
            }}</small></template
          ></Column
        >
        <Column field="module" :header="t('rbac.module')" />
        <Column :header="t('rbac.source')"
          ><template #body="{ data }: { data: PermissionRecord }">{{
            t(`rbac.${data.source}`)
          }}</template></Column
        >
        <Column :header="t('rbac.enforcement')"
          ><template #body="{ data }: { data: PermissionRecord }">{{
            t(`rbac.${data.enforcement}`)
          }}</template></Column
        >
        <Column :header="t('common.status')"
          ><template #body="{ data }: { data: PermissionRecord }"
            ><span class="status-badge" :class="data.status">{{
              t(`rbac.${data.status}`)
            }}</span></template
          ></Column
        >
        <Column :header="t('rbac.actions')"
          ><template #body="{ data }: { data: PermissionRecord }">
            <div v-if="data.source === 'custom' && data.status !== 'archived'" class="actions">
              <Button
                v-if="can('permission.update')"
                type="button"
                variant="text"
                @click="open('edit', data)"
                >{{ t('rbac.edit') }}</Button
              >
              <Button v-if="can('permission.status')" type="button" @click="open('status', data)">{{
                t(data.status === 'active' ? 'rbac.disable' : 'rbac.enable')
              }}</Button>
              <Button
                v-if="can('permission.archive')"
                type="button"
                @click="open('archive', data)"
                >{{ t('rbac.archive') }}</Button
              >
            </div>
            <span v-else class="hint">{{
              t(data.source === 'system' ? 'rbac.protected' : 'rbac.archived')
            }}</span>
          </template></Column
        >
        <template #empty>{{ loading ? t('common.loading') : t('common.noResults') }}</template>
      </Table>
    </section>
    <ModalDialog v-if="action" :title="title" :busy="saving" @close="close">
      <form @submit.prevent="submit">
        <fieldset :disabled="saving" class="permission-form">
          <template v-if="action === 'create' || action === 'edit'">
            <label
              >{{ t('rbac.code')
              }}<Input
                v-model="draft.code"
                :disabled="action === 'edit'"
                required
                maxlength="96"
                pattern="[a-z0-9_]+(\.[a-z0-9_]+)*"
            /></label>
            <p v-if="action === 'create'" class="hint">{{ t('rbac.codeHint') }}</p>
            <label
              >{{ t('rbac.name') }}<Input v-model="draft.name" required maxlength="128"
            /></label>
            <label
              >{{ t('rbac.module')
              }}<Input
                v-model="draft.module"
                :disabled="action === 'edit'"
                required
                maxlength="64"
                pattern="[a-z0-9_]+(\.[a-z0-9_]+)*"
            /></label>
            <label
              >{{ t('rbac.description') }}<Input v-model="draft.description" maxlength="512"
            /></label>
          </template>
          <template v-else-if="action === 'assign'">
            <p class="hint">{{ t('rbac.assignHint') }}</p>
            <label
              >{{ t('common.role')
              }}<Select
                v-model="roleId"
                :options="roles"
                option-label="name"
                option-value="id"
                :aria-label="t('common.role')"
                @change="chooseRole"
            /></label>
            <div v-if="selectedRole" class="permission-options">
              <label
                v-for="permission in assignmentOptions"
                :key="permission.id"
                class="permission-check"
                ><input v-model="assigned" type="checkbox" :value="permission.id" /><span
                  >{{ permission.name
                  }}<span v-if="permission.status === 'disabled'"> · {{ t('rbac.disabled') }}</span
                  ><small class="identity mono">{{ permission.code }}</small></span
                ></label
              >
              <p v-if="!assignmentOptions.length" class="hint">{{ t('rbac.noCustom') }}</p>
            </div>
          </template>
          <p v-else>
            {{
              t(action === 'archive' ? 'rbac.archiveHint' : 'rbac.statusHint', {
                code: selected?.code,
              })
            }}
          </p>
          <p v-if="conflict" class="alert error" role="alert">{{ t('rbac.conflict') }}</p>
          <p v-if="saveError" class="alert error" role="alert">{{ saveError }}</p>
          <div class="actions">
            <Button v-if="conflict" type="button" @click="reloadSelected">{{
              t('rbac.reload')
            }}</Button>
            <Button type="button" @click="close">{{ t('common.cancel') }}</Button>
            <Button
              type="submit"
              variant="primary"
              :disabled="
                conflict ||
                (action === 'assign' && !selectedRole) ||
                (action !== 'create' &&
                  action !== 'assign' &&
                  (selected?.source !== 'custom' || selected?.status === 'archived'))
              "
              >{{ saving ? t('common.processing') : t('common.confirm') }}</Button
            >
          </div>
        </fieldset>
      </form>
    </ModalDialog>
  </div>
</template>
<style scoped>
.permission-form {
  border: 0;
  margin: 0;
  padding: 0;
  display: grid;
  gap: var(--lg-s5);
}
.permission-check {
  flex-direction: row;
  align-items: center;
  gap: var(--lg-s4);
}
.permission-check input[type='checkbox'] {
  width: 18px;
  min-height: 18px;
  height: 18px;
  flex-shrink: 0;
  padding: 0;
  accent-color: var(--accent);
}
.permission-options {
  display: grid;
  gap: var(--lg-s4);
  max-height: 280px;
  overflow: auto;
}
</style>
