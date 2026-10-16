<script setup lang="ts">
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'
import {
  LuminaPanel,
  LuminaButton as Button,
  LuminaInput as Input,
  LuminaSelect as Select,
  LuminaTable as Table,
  LuminaColumn as Column,
} from '../ui/lumina'
import { useRolePermissions, type RolePermissionView } from '../features/roles/useRolePermissions'
const { t } = useI18n()
const {
  roles,
  roleId,
  selectedRole,
  linked,
  visible,
  filters,
  catalogAvailable,
  catalogError,
  listError,
  detailError,
  loading,
  reading,
  effectiveCount,
  load,
  readSelected,
  canManagePermissions,
} = useRolePermissions()
const sources = computed(() => [
  { value: '', label: t('rbac.all') },
  { value: 'system', label: t('rbac.system') },
  { value: 'custom', label: t('rbac.custom') },
])
</script>
<template>
  <div class="data-page role-permissions-page">
    <header class="page-heading">
      <div>
        <h1>{{ t('rolePermissions.title') }}</h1>
        <p>{{ t('rolePermissions.subtitle') }}</p>
      </div>
      <div class="actions">
        <RouterLink to="/roles" class="button lumina-smooth-edge">{{
          t('roleManagement.title')
        }}</RouterLink>
        <RouterLink
          v-if="canManagePermissions"
          to="/permissions"
          class="button lumina-smooth-edge"
          >{{ t('rbac.title') }}</RouterLink
        >
      </div>
    </header>
    <LuminaPanel as="section" class="panel table-panel" radius="var(--lg-r-3xl)" padding="0">
      <form class="filters" @submit.prevent="load">
        <label
          >{{ t('rolePermissions.chooseRole') }}
          <Select
            v-model="roleId"
            :options="roles"
            option-label="name"
            option-value="id"
            :disabled="loading"
            :aria-label="t('rolePermissions.chooseRole')"
          >
            <template #option="{ option }"
              ><div class="lumina-select-option-content">
                <span class="lumina-select-option-label"
                  >{{ option.name }} · {{ t(`rbac.${option.status}`) }}</span
                ><span class="lumina-select-option-detail">{{ option.code }}</span>
              </div></template
            >
          </Select>
        </label>
        <label
          >{{ t('rbac.search')
          }}<Input v-model="filters.search" type="search" :placeholder="t('rbac.searchHint')"
        /></label>
        <label
          >{{ t('rbac.source')
          }}<Select
            v-model="filters.source"
            :options="sources"
            option-label="label"
            option-value="value"
            :disabled="!catalogAvailable"
            :aria-label="t('rbac.source')"
        /></label>
        <label class="role-permissions-check"
          ><input v-model="filters.includeArchived" type="checkbox" :disabled="loading" />{{
            t('rolePermissions.includeArchived')
          }}</label
        >
        <Button type="submit" :disabled="loading">{{ t('common.refresh') }}</Button>
      </form>
      <p v-if="listError" class="alert error" role="alert">{{ listError }}</p>
      <p v-if="catalogError" class="alert error" role="alert">{{ catalogError }}</p>
      <p v-if="selectedRole && !catalogAvailable && !loading && !reading" class="hint">
        {{ t('rolePermissions.idsOnly') }}
      </p>
      <p v-if="detailError" class="alert error" role="alert">
        {{ detailError }}
        <Button type="button" :disabled="reading" @click="readSelected">{{
          t('rbac.reload')
        }}</Button>
      </p>
      <div v-if="selectedRole" class="role-permissions-summary" role="status">
        <div>
          <strong>{{ selectedRole.name }}</strong
          ><span class="mono">{{ selectedRole.code }}</span>
        </div>
        <span class="status-badge" :class="selectedRole.status">{{
          t(`rbac.${selectedRole.status}`)
        }}</span>
        <span>{{ t('rolePermissions.assigned', { count: linked.length }) }}</span>
        <span>{{ t('rolePermissions.effective', { count: effectiveCount ?? '—' }) }}</span>
        <span>{{
          t('rolePermissions.versions', {
            version: selectedRole.version,
            authz: selectedRole.authz_version,
          })
        }}</span>
      </div>
      <p v-if="selectedRole && selectedRole.status !== 'active'" class="hint">
        {{ t('rolePermissions.inactiveRole') }}
      </p>
      <p class="hint">{{ t('rolePermissions.readOnly') }}</p>
      <Table
        :value="visible"
        data-key="id"
        class="admin-table"
        min-width="820px"
        :pt="{ tableContainer: { class: 'table-scroll', 'aria-busy': loading || reading } }"
      >
        <Column :header="t('rbac.name')"
          ><template #body="{ data }: { data: RolePermissionView }"
            ><strong>{{ data.permission?.name ?? t('rolePermissions.unknownPermission') }}</strong
            ><small class="identity mono">{{ data.permission?.code ?? data.id }}</small
            ><small v-if="data.permission?.description" class="identity">{{
              data.permission.description
            }}</small></template
          ></Column
        >
        <Column :header="t('rbac.module')"
          ><template #body="{ data }: { data: RolePermissionView }">{{
            data.permission?.module ?? '—'
          }}</template></Column
        >
        <Column :header="t('rbac.source')"
          ><template #body="{ data }: { data: RolePermissionView }">{{
            data.permission ? t(`rbac.${data.permission.source}`) : '—'
          }}</template></Column
        >
        <Column :header="t('rbac.enforcement')"
          ><template #body="{ data }: { data: RolePermissionView }">{{
            data.permission ? t(`rbac.${data.permission.enforcement}`) : '—'
          }}</template></Column
        >
        <Column :header="t('rolePermissions.permissionStatus')"
          ><template #body="{ data }: { data: RolePermissionView }"
            ><span v-if="data.permission" class="status-badge" :class="data.permission.status">{{
              t(`rbac.${data.permission.status}`)
            }}</span
            ><span v-else>—</span></template
          ></Column
        >
        <Column :header="t('rolePermissions.effectiveState')"
          ><template #body="{ data }: { data: RolePermissionView }"
            ><span class="status-badge" :class="data.effective ? 'active' : 'disabled'">{{
              data.effective === null
                ? t('rolePermissions.unknownState')
                : data.effective
                  ? t('rolePermissions.isEffective')
                  : t('rolePermissions.notEffective')
            }}</span></template
          ></Column
        >
        <template #empty>{{
          loading || reading
            ? t('common.loading')
            : detailError || listError
              ? t('rolePermissions.loadFailed')
              : linked.length
                ? t('common.noResults')
                : t('rolePermissions.noAssignments')
        }}</template>
      </Table>
    </LuminaPanel>
  </div>
</template>
<style scoped>
.role-permissions-summary {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: var(--lg-s5);
  margin-block: var(--lg-s5);
}
.role-permissions-summary > div {
  display: flex;
  align-items: center;
  gap: var(--lg-s4);
}
.role-permissions-check {
  flex-direction: row;
  align-items: center;
  gap: var(--lg-s4);
}
.role-permissions-check input[type='checkbox'] {
  width: 18px;
  min-height: 18px;
  height: 18px;
  flex-shrink: 0;
  padding: 0;
  accent-color: var(--accent);
}
</style>
