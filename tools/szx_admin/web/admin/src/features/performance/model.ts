import type { MachineSample } from '../../api/types'

export function mergeSamples(
  existing: MachineSample[],
  incoming: MachineSample[],
): MachineSample[] {
  const indexed = new Map(existing.map((sample) => [sample.sequence, sample]))
  for (const sample of incoming) indexed.set(sample.sequence, sample)
  const sorted = [...indexed.values()].sort((a, b) => a.sequence - b.sequence)
  const cutoff = (sorted.at(-1)?.sampled_at ?? 0) - 900_000
  return sorted.filter((sample) => sample.sampled_at >= cutoff).slice(-450)
}
export function formatBytes(
  value: number | null,
  locale: string,
  formatter?: Intl.NumberFormat,
): string {
  if (value === null || !Number.isFinite(value) || value < 0) return '—'
  const units = ['B', 'KiB', 'MiB', 'GiB', 'TiB', 'PiB']
  const index =
    value === 0 ? 0 : Math.min(units.length - 1, Math.floor(Math.log(value) / Math.log(1024)))
  return `${(formatter ?? new Intl.NumberFormat(locale, { maximumFractionDigits: 1 })).format(value / 1024 ** index)} ${units[index]}`
}
