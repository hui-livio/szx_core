<script setup lang="ts">
import { computed, ref } from 'vue'
import { useI18n } from 'vue-i18n'
import {
  LuminaButton as Button,
  LuminaInput as Input,
  LuminaSelect as Select,
  LuminaTable as Table,
  LuminaColumn as Column,
} from '../ui/lumina'
import ModalDialog from '../components/ModalDialog.vue'
import { useRoles } from '../features/roles/useRoles'
import type { RoleRecord } from '../api/types'
const { t } = useI18n()
const {
  visible,
  filter,
  loading,
  listError,
  success,
  action,
  selected,
  draft,
  permissions,
  targets,
  preservedIds,
  ready,
  conflict,
  discardRequested,
  task,
  can,
  allowed,
  open,
  load,
  reload,
  close,
  finishClose,
  keepEditing,
  submit,
  requiredPermission,
  requiredScope,
} = useRoles()
const { busy: saving, error: saveError } = task
const permissionSearch = ref('')
function openPermissions(role: RoleRecord) {
  permissionSearch.value = ''
  void open('permissions', role)
}
const title = computed(() =>
  t(
    action.value === 'create'
      ? 'roleManagement.create'
      : action.value === 'edit'
        ? 'roleManagement.edit'
        : action.value === 'permissions'
          ? 'roleManagement.permissions'
          : action.value === 'scopes'
            ? 'roleManagement.scopes'
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
const statuses = computed(() => [
  { value: '', label: t('users.allStatuses') },
  ...['active', 'disabled', 'archived'].map((value) => ({ value, label: t(`rbac.${value}`) })),
])
const groups = computed(() => {
  const grouped = new Map<string, typeof permissions.value>()
  for (const permission of permissions.value) {
    if (
      ![permission.code, permission.name, permission.description, permission.module]
        .join(' ')
        .toLocaleLowerCase()
        .includes(permissionSearch.value.trim().toLocaleLowerCase())
    )
      continue
    const group = grouped.get(permission.module) ?? []
    group.push(permission)
    grouped.set(permission.module, group)
  }
  return [...grouped]
})
</script>
<template>
  <div class="data-page roles-page">
    <header class="page-heading">
      <div>
        <h1>{{ t('roleManagement.title') }}</h1>
        <p>{{ t('roleManagement.subtitle') }}</p>
      </div>
      <div class="actions">
        <RouterLink to="/roles/permissions" class="button lumina-smooth-edge">{{
          t('rolePermissions.viewAssociations')
        }}</RouterLink>
        <Button
          v-if="allowed('create')"
          variant="primary"
          :disabled="saving"
          type="button"
          @click="open('create')"
          >＋ {{ t('roleManagement.create') }}</Button
        >
      </div>
    </header>
    <p v-if="success" class="alert success" role="status">{{ success }}</p>
    <p v-if="saveError && !action" class="alert error" role="alert">{{ saveError }}</p>
    <section class="panel table-panel">
      <form class="filters" @submit.prevent="load">
        <label
          >{{ t('roleManagement.search')
          }}<Input
            v-model="filter.search"
            type="search"
            :placeholder="t('roleManagement.searchHint')"
        /></label>
        <label
          >{{ t('common.status')
          }}<Select
            v-model="filter.status"
            :options="statuses"
            option-label="label"
            option-value="value"
            :aria-label="t('common.status')"
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
        <label class="role-check"
          ><input v-model="filter.includeArchived" type="checkbox" :disabled="loading" />{{
            t('rolePermissions.includeArchived')
          }}</label
        >
        <Button type="submit" :disabled="loading || saving">{{ t('common.refresh') }}</Button>
      </form>
      <p v-if="listError" class="alert error" role="alert">{{ listError }}</p>
      <Table
        :value="visible"
        data-key="id"
        class="admin-table"
        min-width="1000px"
        :pt="{ tableContainer: { class: 'table-scroll', 'aria-busy': loading } }"
      >
        <Column :header="t('roleManagement.name')"
          ><template #body="{ data }: { data: RoleRecord }"
            ><strong>{{ data.name }}</strong
            ><small class="identity mono">{{ data.code }}</small
            ><small class="identity">{{ data.description }}</small></template
          ></Column
        >
        <Column :header="t('rbac.source')"
          ><template #body="{ data }: { data: RoleRecord }">{{
            t(data.system_key ? 'rbac.system' : 'rbac.custom')
          }}</template></Column
        >
        <Column :header="t('common.status')"
          ><template #body="{ data }: { data: RoleRecord }"
            ><span class="status-badge" :class="data.status">{{
              t(`rbac.${data.status}`)
            }}</span></template
          ></Column
        >
        <Column :header="t('roleManagement.userCount')"
          ><template #body="{ data }: { data: RoleRecord }">{{
            data.user_count ?? '—'
          }}</template></Column
        >
        <Column :header="t('roleManagement.permissionCount')"
          ><template #body="{ data }: { data: RoleRecord }">{{
            data.permission_count ?? '—'
          }}</template></Column
        >
        <Column :header="t('rbac.actions')"
          ><template #body="{ data }: { data: RoleRecord }">
            <div class="actions table-actions">
              <RouterLink
                class="user-link"
                :to="{
                  path: '/roles/permissions',
                  query: {
                    role: data.id,
                    archived: data.status === 'archived' ? 'true' : undefined,
                  },
                }"
                >{{ t('roleManagement.view') }}</RouterLink
              >
              <Button
                v-if="allowed('edit', data)"
                type="button"
                variant="text"
                :disabled="saving"
                @click="open('edit', data)"
                >{{ t('rbac.edit') }}</Button
              >
              <Button
                v-if="allowed('permissions', data)"
                type="button"
                variant="text"
                :disabled="saving"
                @click="openPermissions(data)"
                >{{ t('roleManagement.permissions') }}</Button
              >
              <Button
                v-if="allowed('scopes', data)"
                type="button"
                variant="text"
                :disabled="saving"
                @click="open('scopes', data)"
                >{{ t('roleManagement.scopes') }}</Button
              >
              <Button
                v-if="allowed('status', data)"
                type="button"
                variant="text"
                :disabled="saving"
                @click="open('status', data)"
                >{{ t(data.status === 'active' ? 'rbac.disable' : 'rbac.enable') }}</Button
              >
              <Button
                v-if="
                  can('role.archive') &&
                  data.manageable &&
                  !data.system_key &&
                  data.status !== 'archived'
                "
                type="button"
                variant="text"
                :disabled="saving || !allowed('archive', data)"
                @click="open('archive', data)"
                >{{ t('rbac.archive') }}</Button
              >
              <span v-if="data.user_count && !data.system_key" class="hint">{{
                t('roleManagement.inUse')
              }}</span>
            </div>
          </template></Column
        >
        <template #empty>{{
          loading
            ? t('common.loading')
            : listError
              ? t('rolePermissions.loadFailed')
              : t('common.noResults')
        }}</template>
      </Table>
    </section>
    <ModalDialog v-if="action" :title="title" :busy="saving" @close="close">
      <div v-if="discardRequested" class="role-form">
        <p>{{ t('roleManagement.unsaved') }}</p>
        <div class="actions">
          <Button type="button" @click="keepEditing">{{ t('roleManagement.keepEditing') }}</Button
          ><Button type="button" variant="danger" @click="finishClose">{{
            t('roleManagement.discard')
          }}</Button>
        </div>
      </div>
      <form v-else @submit.prevent="submit">
        <fieldset class="role-form" :disabled="saving">
          <p v-if="saving && !ready" class="hint">{{ t('common.loading') }}</p>
          <template v-if="(action === 'create' || action === 'edit') && ready">
            <label
              >{{ t('roleManagement.code')
              }}<Input
                v-model="draft.code"
                required
                minlength="2"
                maxlength="64"
                pattern="[a-z][a-z0-9_]{1,63}"
                :disabled="action === 'edit'"
            /></label>
            <p class="hint">{{ t('roleManagement.codeHint') }}</p>
            <label
              >{{ t('roleManagement.name') }}<Input v-model="draft.name" required maxlength="128"
            /></label>
            <label
              >{{ t('rbac.description') }}<Input v-model="draft.description" maxlength="512"
            /></label>
          </template>
          <template v-else-if="action === 'permissions' && ready">
            <p class="hint">{{ t('roleManagement.authorizationHint') }}</p>
            <label
              >{{ t('rbac.search')
              }}<Input v-model="permissionSearch" type="search" :placeholder="t('rbac.searchHint')"
            /></label>
            <div class="role-options lumina-scrollbar">
              <section v-for="[module, items] in groups" :key="module">
                <h3>{{ module }}</h3>
                <label v-for="permission in items" :key="permission.id" class="role-check">
                  <input
                    v-model="draft.ids"
                    type="checkbox"
                    :value="permission.id"
                    :disabled="requiredPermission(permission) || permission.status === 'archived'"
                  />
                  <span
                    >{{ permission.name }}
                    <small
                      >{{ t(`rbac.${permission.source}`) }} ·
                      {{ t(`rbac.${permission.enforcement}`) }} · {{ t(`rbac.${permission.status}`)
                      }}{{
                        requiredPermission(permission) ? ' · ' + t('rbac.protected') : ''
                      }}</small
                    ><small class="identity mono">{{ permission.code }}</small></span
                  >
                </label>
              </section>
              <p v-if="!groups.length" class="hint">{{ t('common.noResults') }}</p>
            </div>
          </template>
          <template v-else-if="action === 'scopes' && ready">
            <p class="hint">{{ t('roleManagement.scopeHint') }}</p>
            <div class="role-options lumina-scrollbar">
              <label v-for="target in targets" :key="target.id" class="role-check"
                ><input
                  v-model="draft.ids"
                  type="checkbox"
                  :value="target.id"
                  :disabled="requiredScope()"
                /><span
                  >{{ target.name }} · {{ t(`rbac.${target.status}`)
                  }}<small class="identity mono">{{ target.code }}</small></span
                ></label
              >
            </div>
            <p v-if="requiredScope()" class="hint">{{ t('roleManagement.scopeFloor') }}</p>
          </template>
          <template v-else-if="action === 'archive' && ready">
            <p>{{ t('roleManagement.archiveHint', { code: selected?.code }) }}</p>
            <label
              >{{ t('roleManagement.confirmCode')
              }}<Input v-model="draft.confirmation" required autocomplete="off"
            /></label>
          </template>
          <p v-else-if="action === 'status' && ready">
            {{ t('roleManagement.statusHint', { code: selected?.code }) }}
          </p>
          <p
            v-if="preservedIds.length && (action === 'permissions' || action === 'scopes')"
            class="hint"
          >
            {{ t('roleManagement.preserveUnknown', { count: preservedIds.length }) }}
          </p>
          <p v-if="conflict" class="alert error" role="alert">{{ t('rbac.conflict') }}</p>
          <p v-if="saveError" class="alert error" role="alert">{{ saveError }}</p>
          <div class="actions">
            <Button
              v-if="action !== 'create' && (conflict || !ready)"
              type="button"
              @click="reload"
              >{{ t('rbac.reload') }}</Button
            >
            <Button type="button" @click="close">{{ t('common.cancel') }}</Button>
            <Button
              type="submit"
              variant="primary"
              :disabled="
                !ready ||
                !allowed(action, selected) ||
                conflict ||
                (action === 'archive' && draft.confirmation !== selected?.code)
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
.role-form {
  border: 0;
  margin: 0;
  padding: 0;
  display: grid;
  gap: var(--lg-s5);
}
.role-options {
  max-height: min(40dvh, 360px);
  overflow: auto;
  overscroll-behavior: contain;
  display: grid;
  gap: var(--lg-s4);
}
.role-check {
  display: flex;
  flex-direction: row;
  align-items: center;
  gap: var(--lg-s4);
}
.role-check input[type='checkbox'] {
  width: 18px;
  height: 18px;
  min-height: 18px;
  flex-shrink: 0;
  padding: 0;
  accent-color: var(--accent);
}
.role-options .role-check {
  padding-block: var(--lg-s3);
}
.role-options h3 {
  font-size: var(--lg-fs-md);
}
</style>
