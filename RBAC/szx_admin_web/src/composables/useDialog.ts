import { onMounted, useTemplateRef } from 'vue'
export function useDialog() {
  const dialog = useTemplateRef<HTMLDialogElement>('dialog')
  onMounted(() => dialog.value?.showModal())
  return { dialog }
}
