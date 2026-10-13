// DatePicker emits local Date objects; never serialize these through toISOString().
export function formatDateTimeLocal(value: Date | null): string {
  if (!value || Number.isNaN(value.getTime())) return ''
  const pad = (part: number) => String(part).padStart(2, '0')
  return `${String(value.getFullYear()).padStart(4, '0')}-${pad(value.getMonth() + 1)}-${pad(value.getDate())}T${pad(value.getHours())}:${pad(value.getMinutes())}:${pad(value.getSeconds())}`
}

export function parseDateTimeLocal(value: string): Date | null {
  if (!/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}$/.test(value)) return null
  const date = new Date(value)
  return formatDateTimeLocal(date) === value ? date : null
}
