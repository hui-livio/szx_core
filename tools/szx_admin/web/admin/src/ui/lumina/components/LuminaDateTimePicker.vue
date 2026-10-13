<script setup lang="ts">
import DatePicker from 'primevue/datepicker'
import { computed, nextTick, shallowRef, useId, watch } from 'vue'
import { formatDateTimeLocal, parseDateTimeLocal } from '../utils/dateTime'

const model = defineModel<string>({ required: true })
const props = defineProps<{ label: string; invalidText: string }>()
const inputId = useId()
const invalid = shallowRef(false)
const date = computed(() => parseDateTimeLocal(model.value))

function validateInput() {
  const input = document.getElementById(inputId) as HTMLInputElement | null
  if (!input) return
  const expected = formatDateTimeLocal(date.value).replace('T', ' ')
  invalid.value = !!input.value && input.value !== expected
  input.setCustomValidity(invalid.value ? props.invalidText : '')
}

// PrimeVue 4.5.5's manual parser expects AM/PM even for 24-hour time.
// Parse the documented local format here without patching private component methods.
function inputDate(event: Event) {
  const input = event.target as HTMLInputElement
  const raw = input.value
  const value = parseDateTimeLocal(raw.replace(' ', 'T'))
  if (value || !raw) model.value = formatDateTimeLocal(value)
  invalid.value = !!raw && !value
  input.setCustomValidity(invalid.value ? props.invalidText : '')
  void nextTick(() => {
    // Keep incomplete input visible when the DatePicker rerenders its InputText.
    input.value = raw
    validateInput()
  })
}

function selectDate(value: Date | Date[] | (Date | null)[] | null | undefined) {
  if (Array.isArray(value)) return
  invalid.value = false
  model.value = formatDateTimeLocal(value ?? null)
  void nextTick(() => {
    const input = document.getElementById(inputId) as HTMLInputElement | null
    if (input) input.value = model.value.replace('T', ' ')
    validateInput()
  })
}

watch([model, () => props.invalidText], () => void nextTick(validateInput))
</script>

<template>
  <DatePicker
    :model-value="date"
    :input-id="inputId"
    :aria-label="label"
    :invalid="invalid"
    date-format="yy-mm-dd"
    show-time
    show-seconds
    hour-format="24"
    show-icon
    icon-display="input"
    show-button-bar
    :show-on-focus="false"
    placeholder="YYYY-MM-DD HH:mm:ss"
    :pt="{
      panel: { class: 'lumina-datetime-panel' },
      pcInputText: { root: { class: 'lumina-smooth-edge' } },
    }"
    @update:model-value="selectDate"
    @input="inputDate"
    @blur="validateInput"
  />
</template>
