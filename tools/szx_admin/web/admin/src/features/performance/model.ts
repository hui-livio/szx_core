import type { MachineSample, MachineSnapshot } from '../../api/types'

export interface NetworkRateSample {
  sequence: number
  sampled_at: number
  rates: Record<string, { rx: number | null; tx: number | null }>
}

export function appendNetworkSample(
  existing: NetworkRateSample[],
  snapshot: MachineSnapshot,
): NetworkRateSample[] {
  if (existing.length && snapshot.sequence <= existing[existing.length - 1]!.sequence)
    return existing
  const rates: NetworkRateSample['rates'] = {}
  for (const network of snapshot.networks ?? []) {
    rates[network.id] = {
      rx: network.rx_bytes_per_sec,
      tx: network.tx_bytes_per_sec,
    }
  }
  const next = [
    ...existing,
    { sequence: snapshot.sequence, sampled_at: snapshot.sampled_at, rates },
  ]
  const cutoff = snapshot.sampled_at - 900_000
  return next.filter((sample) => sample.sampled_at >= cutoff).slice(-450)
}

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

export function displayMetricText(value: string | null | undefined): string {
  const text = value?.trim()
  return text && text.toLowerCase() !== 'undefined' && text.toLowerCase() !== 'null' ? text : '—'
}
