import { getInstanceByDom } from 'echarts/core'

/** Browser-only test probe. Not imported by the production application. */
export function trackChartUpdates() {
  const hosts = [...document.querySelectorAll<HTMLElement>('.lumina-chart')]
  const chart = hosts.length ? getInstanceByDom(hosts[0]!) : undefined
  if (!chart) throw new Error('Mount a chart before installing the update probe')
  for (const host of hosts) host.dataset.setOptionCount = '0'
  const prototype = Object.getPrototypeOf(chart) as typeof chart
  const original = prototype.setOption
  prototype.setOption = new Proxy(original, {
    apply(target, instance: typeof chart, args) {
      const host = instance.getDom()
      host.dataset.setOptionCount = String(Number(host.dataset.setOptionCount ?? 0) + 1)
      return Reflect.apply(target, instance, args)
    },
  })
}

export function lineSmoothSettings(label: string): boolean[] {
  const host = [...document.querySelectorAll<HTMLElement>('.lumina-chart')].find(
    (element) => element.getAttribute('aria-label') === label,
  )
  const chart = host ? getInstanceByDom(host) : undefined
  if (!chart) throw new Error(`Chart not mounted: ${label}`)
  const series = chart.getOption().series
  return Array.isArray(series)
    ? series.map((item) => Boolean((item as { smooth?: boolean }).smooth))
    : []
}
