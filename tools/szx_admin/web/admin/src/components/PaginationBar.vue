<script setup lang="ts">
import Paginator from 'primevue/paginator'
import { useI18n } from 'vue-i18n'
const { t } = useI18n()
defineProps<{
  page: number
  pageSize: number
  pageCount: number
  total: number
  busy: boolean
}>()
const emit = defineEmits<{ change: [page: number] }>()
</script>
<template>
  <footer class="pagination" :aria-label="t('common.pagination')">
    <span>{{ t('common.totalRecords', { count: total }) }}</span>
    <fieldset :disabled="busy" :aria-busy="busy" class="pagination-controls">
      <Paginator
        :first="(page - 1) * pageSize"
        :rows="pageSize"
        :total-records="total"
        template="PrevPageLink CurrentPageReport NextPageLink"
        :current-page-report-template="`${page} / ${pageCount}`"
        :aria-label="t('common.pagination')"
        @page="!busy && emit('change', $event.page + 1)"
      >
        <template #previcon>{{ t('common.previous') }}</template>
        <template #nexticon>{{ t('common.next') }}</template>
      </Paginator>
    </fieldset>
  </footer>
</template>
