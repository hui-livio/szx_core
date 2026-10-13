<script setup lang="ts">
import {
  LuminaButton as Button,
  LuminaInput as InputText,
  LuminaSelect as Select,
} from '../../ui/lumina'
import { computed } from 'vue'
import type { Role, User } from '../../api/types'
import type { UserAction, UserDraft } from '../../composables/useUsersViewModel'
import { roleLabels, statusLabels, formatTime } from '../../domain'
import { useI18n } from 'vue-i18n'
const { t } = useI18n()
const draft = defineModel<UserDraft>({ required: true })
const props = defineProps<{
  action: UserAction
  user: User | null
  roles: Role[]
  passwordHint: string
  busy: boolean
  error: string
  conflict: boolean
  targetStatus: string
}>()
const emit = defineEmits<{ submit: []; close: []; reload: [] }>()
const roleOptions = computed(() =>
  props.roles.map((value) => ({ value, label: t(roleLabels[value]) })),
)
</script>
<template>
  <form class="form-stack" @submit.prevent="emit('submit')">
    <p v-if="error" class="alert error" role="alert">{{ error }}</p>
    <Button
      v-if="conflict || (error && user)"
      type="button"
      :disabled="busy"
      @click="emit('reload')"
    >
      {{ t('users.reload') }}
    </Button>
    <label v-if="action === 'create'"
      >{{ t('users.identity')
      }}<InputText
        v-model="draft.identity"
        required
        maxlength="64"
        autocomplete="off"
        :disabled="busy"
    /></label>
    <p v-if="action === 'create'" class="hint">{{ t('users.identityImmutable') }}</p>
    <label v-if="action === 'create' || action === 'name'"
      >{{ t('users.displayName')
      }}<InputText v-model="draft.displayName" required maxlength="128" :disabled="busy"
    /></label>
    <label v-if="action === 'create' || action === 'role'"
      >{{ t('common.role')
      }}<Select
        v-model="draft.role"
        :options="roleOptions"
        option-label="label"
        option-value="value"
        :aria-label="t('common.role')"
        :disabled="busy"
    /></label>
    <template v-if="action === 'create' || action === 'reset'">
      <label
        >{{ action === 'create' ? t('users.initialPassword') : t('users.newPassword')
        }}<InputText
          v-model="draft.password"
          name="initial-password"
          type="password"
          autocomplete="new-password"
          aria-describedby="initial-password-rule"
          :disabled="busy"
      /></label>
      <p id="initial-password-rule" class="hint">
        {{ t('users.optionalPassword', { hint: passwordHint }) }}
      </p>
      <label
        >{{ action === 'create' ? t('users.confirmInitialPassword') : t('users.confirmNewPassword')
        }}<InputText
          v-model="draft.confirmation"
          name="confirm-initial-password"
          type="password"
          autocomplete="new-password"
          :disabled="busy"
      /></label>
    </template>
    <template v-if="user"
      ><p v-if="action !== 'details'" class="hint">
        {{ t('users.target', { name: user.display_name, identifier: user.login_identifier }) }}
      </p>
      <dl v-if="action === 'details'" class="details-grid">
        <div>
          <dt>{{ t('common.userId') }}</dt>
          <dd class="mono">{{ user.id }}</dd>
        </div>
        <div>
          <dt>{{ t('common.loginIdentifier') }}</dt>
          <dd>{{ user.login_identifier }}</dd>
        </div>
        <div>
          <dt>{{ t('users.displayName') }}</dt>
          <dd>{{ user.display_name }}</dd>
        </div>
        <div>
          <dt>{{ t('users.roleStatus') }}</dt>
          <dd>{{ t(roleLabels[user.role]) }} / {{ t(statusLabels[user.status]) }}</dd>
        </div>
        <div>
          <dt>{{ t('common.createdAt') }}</dt>
          <dd>{{ formatTime(user.created_at) }}</dd>
        </div>
        <div>
          <dt>{{ t('common.updatedAt') }}</dt>
          <dd>{{ formatTime(user.updated_at) }}</dd>
        </div>
        <div>
          <dt>{{ t('common.lastLogin') }}</dt>
          <dd>{{ formatTime(user.last_login_at) }}</dd>
        </div>
        <div>
          <dt>{{ t('common.version') }}</dt>
          <dd>{{ user.version }}</dd>
        </div>
      </dl></template
    >
    <p v-if="action === 'status'" class="alert warning">
      {{ targetStatus === 'disabled' ? t('users.disableWarning') : t('users.enableWarning') }}
    </p>
    <p v-if="action === 'unlock'" class="alert info">{{ t('users.unlockInfo') }}</p>
    <p v-if="action === 'role'" class="alert warning">{{ t('users.roleWarning') }}</p>
    <p v-if="action === 'reset'" class="alert warning">
      {{ t('users.resetWarning') }}
    </p>
    <div class="modal-actions">
      <Button type="button" :disabled="busy" @click="emit('close')">
        {{ action === 'details' ? t('common.close') : t('common.cancel') }}</Button
      ><Button
        v-if="action !== 'details'"
        class="primary"
        :disabled="busy || conflict || (!!error && !!user && action !== 'reset')"
        type="submit"
        variant="primary"
      >
        {{ busy ? t('common.processing') : t('common.confirm') }}
      </Button>
    </div>
  </form>
</template>
