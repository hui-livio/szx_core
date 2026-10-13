<script setup lang="ts">
import { LuminaPagination } from '../ui/lumina'
import { useI18n } from 'vue-i18n'
const { t } = useI18n()
defineProps<{ page: number; pageSize: number; pageCount: number; total: number; busy: boolean }>()
const emit = defineEmits<{ change: [page: number] }>()
</script>
<template>
  <LuminaPagination
    :page="page"
    :page-size="pageSize"
    :page-count="pageCount"
    :total="total"
    :busy="busy"
    :summary="t('common.totalRecords', { count: total })"
    :label="t('common.pagination')"
    :previous-label="t('common.previous')"
    :next-label="t('common.next')"
    @change="emit('change', $event)"
  />
</template>
