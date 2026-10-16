import type { ComposeOption } from 'echarts/core'
import type { LineSeriesOption, PieSeriesOption } from 'echarts/charts'
import type { GridComponentOption, TooltipComponentOption } from 'echarts/components'

export type LuminaChartOption = ComposeOption<
  LineSeriesOption | PieSeriesOption | GridComponentOption | TooltipComponentOption
>
