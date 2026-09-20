<script setup lang="ts">
import type { Role, User } from '../../api/types'
import type { UserAction, UserDraft } from '../../composables/useUsersViewModel'
import { roleLabels, statusLabels, formatTime } from '../../domain'
const draft = defineModel<UserDraft>({ required: true })
defineProps<{
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
</script>
<template>
  <form class="form-stack" @submit.prevent="emit('submit')">
    <p v-if="error" class="alert error" role="alert">{{ error }}</p>
    <button
      v-if="conflict || (error && user)"
      type="button"
      :disabled="busy"
      @click="emit('reload')"
    >
      重新加载用户
    </button>
    <template v-if="action === 'create'"
      ><label
        >登录标识类型<select v-model="draft.identityType" :disabled="busy">
          <option value="username">用户名</option>
          <option value="uid">UID</option>
        </select></label
      ><label
        >{{ draft.identityType === 'uid' ? 'UID' : '用户名'
        }}<input
          v-model="draft.identity"
          required
          maxlength="64"
          autocomplete="off"
          :disabled="busy"
      /></label>
      <p class="hint">登录标识创建后不可修改。</p></template
    >
    <label v-if="action === 'create' || action === 'name'"
      >展示名<input v-model="draft.displayName" required maxlength="128" :disabled="busy"
    /></label>
    <label v-if="action === 'create' || action === 'role'"
      >角色<select v-model="draft.role" aria-label="角色" :disabled="busy">
        <option v-for="role in roles" :key="role" :value="role">{{ roleLabels[role] }}</option>
      </select></label
    >
    <template v-if="action === 'create' || action === 'reset'">
      <label
        >{{ action === 'create' ? '初始密码' : '新密码'
        }}<input
          v-model="draft.password"
          name="initial-password"
          type="password"
          autocomplete="new-password"
          aria-describedby="initial-password-rule"
          :disabled="busy"
      /></label>
      <p id="initial-password-rule" class="hint">
        选填，{{ passwordHint }}；留空则自动生成有效期为 24 小时的临时密码。
      </p>
      <label
        >{{ action === 'create' ? '确认初始密码' : '确认新密码'
        }}<input
          v-model="draft.confirmation"
          name="confirm-initial-password"
          type="password"
          autocomplete="new-password"
          :disabled="busy"
      /></label>
    </template>
    <template v-if="user"
      ><p v-if="action !== 'details'" class="hint">
        目标用户：{{ user.display_name }}（{{ user.uid ?? user.username }}）
      </p>
      <dl v-if="action === 'details'" class="details-grid">
        <div>
          <dt>用户 ID</dt>
          <dd class="mono">{{ user.id }}</dd>
        </div>
        <div>
          <dt>登录标识</dt>
          <dd>{{ user.uid ?? user.username }}</dd>
        </div>
        <div>
          <dt>展示名</dt>
          <dd>{{ user.display_name }}</dd>
        </div>
        <div>
          <dt>角色 / 状态</dt>
          <dd>{{ roleLabels[user.role] }} / {{ statusLabels[user.status] }}</dd>
        </div>
        <div>
          <dt>创建时间</dt>
          <dd>{{ formatTime(user.created_at) }}</dd>
        </div>
        <div>
          <dt>更新时间</dt>
          <dd>{{ formatTime(user.updated_at) }}</dd>
        </div>
        <div>
          <dt>最近登录</dt>
          <dd>{{ formatTime(user.last_login_at) }}</dd>
        </div>
        <div>
          <dt>版本</dt>
          <dd>{{ user.version }}</dd>
        </div>
      </dl></template
    >
    <p v-if="action === 'status'" class="alert warning">
      {{
        targetStatus === 'disabled'
          ? '禁用后，此用户将无法登录，现有会话也会失效。'
          : '启用后，此用户可以重新登录。'
      }}
    </p>
    <p v-if="action === 'unlock'" class="alert info">解锁后清除失败登录计数，用户可以重新登录。</p>
    <p v-if="action === 'role'" class="alert warning">角色变更会使该用户的现有会话失效。</p>
    <p v-if="action === 'reset'" class="alert warning">
      重置后旧密码和现有会话失效。填写新密码则使用指定密码；留空则随机生成临时密码，并在下一步展示一次。
    </p>
    <div class="modal-actions">
      <button type="button" :disabled="busy" @click="emit('close')">
        {{ action === 'details' ? '关闭' : '取消' }}</button
      ><button
        v-if="action !== 'details'"
        class="primary"
        :disabled="busy || conflict || (!!error && !!user && action !== 'reset')"
      >
        {{ busy ? '处理中…' : '确认' }}
      </button>
    </div>
  </form>
</template>
