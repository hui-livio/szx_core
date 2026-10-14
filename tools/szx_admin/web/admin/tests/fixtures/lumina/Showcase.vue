<script setup lang="ts">
import { ref } from 'vue'
import {
  LuminaButton,
  LuminaMark,
  LuminaPanel,
  LuminaCard,
  LuminaCardGrid,
  LuminaChart,
  LuminaInput,
  LuminaSelect,
  LuminaTable,
  LuminaColumn,
  LuminaDialog,
  LuminaDateTimePicker,
  LuminaPagination,
  LuminaBackground,
  useLuminaScale,
} from '../../../src/ui/lumina'

useLuminaScale()
const gridCount = ref(7)
const gridNarrow = ref(false)
const name = ref('')
const colour = ref('blue')
const customChoice = ref('')
const submissions = ref(0)
const date = ref('')
const page = ref(1)
const open = ref(false)
const busy = ref(false)
const options = [
  { value: 'blue', label: 'Blue' },
  { value: 'violet', label: 'Violet' },
]
const rows = [
  { id: 1, name: 'Alpha' },
  { id: 2, name: 'Beta' },
]
</script>
<template>
  <LuminaBackground />
  <main class="showcase">
    <h1>Standalone Lumina controls</h1>
    <LuminaMark label="Reusable S mark" :size="48" :radius="6" :font-size="24" :font-weight="400" />
    <LuminaMark label="Reusable user mark" text="U" variant="soft" :size="48" radius="50%" />
    <LuminaPanel data-testid="glass-panel" :radius="18" :padding="16"
      ><p>Reusable glass panel slot</p></LuminaPanel
    >
    <LuminaPanel data-testid="solid-panel" variant="solid" as="section" :radius="10" :padding="12"
      ><p>Reusable solid panel slot</p></LuminaPanel
    >
    <LuminaCard
      data-testid="reusable-card"
      title="Reusable card"
      subtitle="Caller subtitle"
      :radius="18"
      :padding="16"
    >
      <template #actions><LuminaButton label="Card action" /></template>
      <p>Caller body</p>
      <LuminaChart
        :option="{
          series: [{ type: 'line', data: [1, 2, 3] }],
          xAxis: { type: 'category', data: ['A', 'B', 'C'] },
          yAxis: { type: 'value' },
        }"
        label="Reusable chart"
      />
      <template #footer>Caller footer</template>
    </LuminaCard>
    <LuminaCard
      data-testid="unavailable-card"
      title="Unavailable card"
      state="unavailable"
      state-label="Caller unavailable"
    />
    <div class="actions">
      <LuminaButton label="Toggle grid width" @click="gridNarrow = !gridNarrow" />
      <LuminaButton label="Remove grid card" @click="gridCount = Math.max(0, gridCount - 1)" />
    </div>
    <div :style="{ width: gridNarrow ? '260px' : '100%', maxWidth: '100%' }">
      <LuminaCardGrid
        v-slot="{ cellStyle }"
        data-testid="adaptive-grid"
        :count="gridCount"
        :min-card-width="180"
        :gap="12"
      >
        <LuminaCard
          v-for="index in gridCount"
          :key="index"
          :style="cellStyle(index - 1)"
          :title="`Grid card ${index}`"
          >Caller content</LuminaCard
        >
      </LuminaCardGrid>
    </div>
    <form @submit.prevent="submissions++">
      <label>Name<LuminaInput v-model="name" aria-label="Name" required /></label>
      <LuminaButton label="Submit" type="submit" variant="primary">
        <template #icon><span data-testid="submit-icon" aria-hidden="true">✓</span></template>
      </LuminaButton>
      <output data-testid="submissions">{{ submissions }}</output>
    </form>
    <LuminaSelect
      v-model="colour"
      :options="options"
      option-label="label"
      option-value="value"
      aria-label="Colour"
    >
      <template #value="{ value }">Shade: {{ value }}</template>
    </LuminaSelect>
    <LuminaSelect
      v-model="customChoice"
      editable
      :options="options"
      option-label="label"
      option-value="value"
      aria-label="Editable choice"
      :pt="{ label: { maxlength: 128 }, listContainer: { class: 'lumina-scrollbar' } }"
    >
      <template #option="{ option }">
        <div class="lumina-select-option-content">
          <span class="lumina-select-option-label">{{ option.label }}</span>
          <span class="lumina-select-option-detail">{{ option.value }}</span>
        </div>
      </template>
    </LuminaSelect>
    <output data-testid="editable-choice-model">{{ customChoice }}</output>
    <LuminaDateTimePicker
      v-model="date"
      label="Local time"
      invalid-text="Enter a valid date and time."
    />
    <output data-testid="local-time-model">{{ date }}</output>
    <LuminaTable :value="rows" data-key="id" min-width="400px">
      <LuminaColumn header="Name"
        ><template #body="{ data }: { data: { name: string } }"
          >Row: {{ data.name }}</template
        ></LuminaColumn
      >
      <template #empty>No rows</template>
    </LuminaTable>
    <LuminaPagination
      :page="page"
      :page-size="1"
      :page-count="3"
      :total="3"
      :busy="busy"
      summary="Three records"
      label="Pages"
      previous-label="Previous"
      next-label="Next"
      @change="page = $event"
    />
    <LuminaButton variant="primary" @click="open = true">Open dialog</LuminaButton>
    <LuminaButton @click="busy = !busy">Toggle busy</LuminaButton>
    <LuminaDialog
      v-if="open"
      title="Reusable dialog"
      close-label="Close"
      :busy="busy"
      @close="open = false"
    >
      <p>Business-independent dialog content</p>
      <template #footer><span data-testid="dialog-footer">Custom footer slot</span></template>
    </LuminaDialog>
  </main>
</template>
<style scoped>
.showcase {
  max-width: 680px;
  margin: 24px auto;
  padding: 24px;
  display: grid;
  gap: 16px;
}
form {
  display: flex;
  align-items: end;
  gap: 12px;
}
label {
  display: grid;
  gap: 6px;
}
</style>
