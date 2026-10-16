<script setup lang="ts">
import { LuminaPagination } from '../ui/lumina'
import { useI18n } from 'vue-i18n'
const { t } = useI18n()
defineProps<{ page: number; pageSize: number; pageCount: number; total: number; busy: boolean }>()
const emit = defineEmits<{ change: [page: number]; 'change:size': [size: number] }>()
</script>
<template>
  <LuminaPagination
    :page="page"
    :page-size="pageSize"
    :page-count="pageCount"
    :total="total"
    :busy="busy"
    :page-size-options="[10, 20, 50, 100]"
    :min-page-size="1"
    :max-page-size="100"
    :page-size-label="t('common.pageSize')"
    :page-size-custom-label="t('common.customPageSize')"
    :page-size-apply-label="t('common.applyPageSize')"
    :page-size-hint="t('common.pageSizeHint', { min: 1, max: 100 })"
    :summary="t('common.totalRecords', { count: total })"
    :label="t('common.pagination')"
    :previous-label="t('common.previous')"
    :next-label="t('common.next')"
    @change="emit('change', $event)"
    @change:size="emit('change:size', $event)"
  />
</template>
