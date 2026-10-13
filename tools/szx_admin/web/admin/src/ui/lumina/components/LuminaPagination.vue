<script setup lang="ts">
import Paginator from 'primevue/paginator'
import LuminaButton from './LuminaButton.vue'

defineProps<{
  page: number
  pageSize: number
  pageCount: number
  total: number
  busy?: boolean
  summary: string
  label: string
  previousLabel: string
  nextLabel: string
}>()
const emit = defineEmits<{ change: [page: number] }>()
</script>
<template>
  <footer class="pagination" :aria-label="label">
    <span>{{ summary }}</span>
    <fieldset :disabled="busy" :aria-busy="busy" class="pagination-controls">
      <Paginator
        :first="(page - 1) * pageSize"
        :rows="pageSize"
        :total-records="total"
        :aria-label="label"
        @page="!busy && emit('change', $event.page + 1)"
      >
        <template #container="{ prevPageCallback, nextPageCallback }">
          <div class="lumina-pagination-actions">
            <LuminaButton
              class="p-paginator-prev"
              :disabled="busy || page <= 1"
              :aria-label="previousLabel"
              @click="prevPageCallback"
              >{{ previousLabel }}</LuminaButton
            >
            <span class="p-paginator-current">{{ page }} / {{ pageCount }}</span>
            <LuminaButton
              class="p-paginator-next"
              :disabled="busy || page >= pageCount"
              :aria-label="nextLabel"
              @click="nextPageCallback"
              >{{ nextLabel }}</LuminaButton
            >
          </div>
        </template>
      </Paginator>
    </fieldset>
  </footer>
</template>
