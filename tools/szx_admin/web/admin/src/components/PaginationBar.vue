<script setup lang="ts">
import { useI18n } from 'vue-i18n'
const { t } = useI18n()
defineProps<{ page: number; pageCount: number; total: number; busy: boolean }>()
const emit = defineEmits<{ change: [page: number] }>()
</script>
<template>
  <footer class="pagination" :aria-label="t('common.pagination')">
    <span>{{ t('common.totalRecords', { count: total }) }}</span>
    <div class="actions">
      <button :disabled="busy || page <= 1" @click="emit('change', page - 1)">
        {{ t('common.previous') }}</button
      ><span>{{ page }} / {{ pageCount }}</span
      ><button :disabled="busy || page >= pageCount" @click="emit('change', page + 1)">
        {{ t('common.next') }}
      </button>
    </div>
  </footer>
</template>
