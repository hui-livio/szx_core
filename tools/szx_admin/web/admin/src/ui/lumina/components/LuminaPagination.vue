<script setup lang="ts">
import { computed, ref, useId, watch } from 'vue'
import LuminaSelect from './LuminaSelect.vue'
import LuminaInput from './LuminaInput.vue'
import { parseLuminaPageSize } from '../utils/pageSize'
import Paginator from 'primevue/paginator'
import LuminaButton from './LuminaButton.vue'

const props = withDefaults(
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
    pageSizeOptions?: readonly number[]
    pageSizeLabel?: string
    pageSizeApplyLabel?: string
    pageSizeCustomLabel?: string
    pageSizeHint?: string
    minPageSize?: number
    maxPageSize?: number
  }>(),
  { pageSizeOptions: () => [], minPageSize: 1, maxPageSize: Number.MAX_SAFE_INTEGER },
)
const emit = defineEmits<{ change: [page: number]; 'change:size': [size: number] }>()
const sizeDraft = ref<string | null>(String(props.pageSize))
const customSize = ref(false)
const sizeHintId = `${useId()}-page-size-hint`
const parsedSize = computed(() =>
  parseLuminaPageSize(sizeDraft.value, props.minPageSize, props.maxPageSize),
)
const sizeOptions = computed(() => [
  ...props.pageSizeOptions
    .filter((size) => parseLuminaPageSize(size, props.minPageSize, props.maxPageSize) !== null)
    .map((size) => ({ label: String(size), value: size as number | string })),
  ...(props.pageSizeCustomLabel ? [{ label: props.pageSizeCustomLabel, value: 'custom' }] : []),
])
const selectedSize = computed(() =>
  customSize.value || !props.pageSizeOptions.includes(props.pageSize) ? 'custom' : props.pageSize,
)
watch(
  () => props.pageSize,
  (value) => {
    sizeDraft.value = String(value)
  },
)

function applyPageSize() {
  const size = parsedSize.value
  if (props.busy || size === null || size === props.pageSize) return
  emit('change:size', size)
}
function updatePageSize(value: unknown) {
  if (props.busy) return
  if (value === 'custom') {
    customSize.value = true
    sizeDraft.value = String(props.pageSize)
  } else if (typeof value === 'number') {
    customSize.value = false
    sizeDraft.value = String(value)
    applyPageSize()
  }
}
</script>
<template>
  <footer class="pagination" :aria-label="label">
    <span>{{ summary }}</span>
    <div class="pagination-actions">
      <div v-if="pageSizeLabel" class="pagination-size">
        <span>{{ pageSizeLabel }}</span>
        <LuminaSelect
          :model-value="selectedSize"
          :options="sizeOptions"
          option-label="label"
          option-value="value"
          :aria-label="pageSizeLabel"
          :disabled="busy"
          @update:model-value="updatePageSize"
        />
        <LuminaInput
          v-if="selectedSize === 'custom'"
          v-model="sizeDraft"
          class="pagination-size-input"
          type="number"
          inputmode="numeric"
          :min="minPageSize"
          :max="maxPageSize"
          step="1"
          :aria-label="pageSizeCustomLabel"
          :aria-describedby="parsedSize === null && pageSizeHint ? sizeHintId : undefined"
          :aria-invalid="parsedSize === null"
          :invalid="parsedSize === null"
          :title="pageSizeHint"
          :disabled="busy"
          @keydown.enter.prevent="applyPageSize"
        />
        <LuminaButton
          v-if="selectedSize === 'custom'"
          class="pagination-size-apply"
          :disabled="busy || parsedSize === null || parsedSize === pageSize"
          :aria-label="pageSizeApplyLabel"
          @click="applyPageSize"
          >{{ pageSizeApplyLabel }}</LuminaButton
        >
        <small v-if="parsedSize === null && pageSizeHint" :id="sizeHintId" role="status">
          {{ pageSizeHint }}
        </small>
      </div>
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
    </div>
  </footer>
</template>
