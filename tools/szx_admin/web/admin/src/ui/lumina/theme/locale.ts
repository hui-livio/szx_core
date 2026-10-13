import type { LuminaLocale } from './install'

export interface LuminaCalendarLabels {
  today: string
  clear: string
  chooseDate: string
  chooseMonth: string
  chooseYear: string
  navigation: Record<string, string>
}

export function createLuminaCalendarLocale(
  locale: string,
  labels: LuminaCalendarLabels,
  options: { firstDayOfWeek?: number } = {},
): LuminaLocale {
  const names = (field: 'weekday' | 'month', width: 'long' | 'short' | 'narrow', count: number) => {
    const formatter = new Intl.DateTimeFormat(locale, { [field]: width, timeZone: 'UTC' })
    // January 1, 2023 is Sunday; PrimeVue expects its day-name arrays to start there.
    return Array.from({ length: count }, (_, index) =>
      formatter.format(
        new Date(
          Date.UTC(2023, field === 'month' ? index : 0, field === 'weekday' ? index + 1 : 1),
        ),
      ),
    )
  }
  return {
    dayNames: names('weekday', 'long', 7),
    dayNamesShort: names('weekday', 'short', 7),
    dayNamesMin: names('weekday', 'narrow', 7),
    monthNames: names('month', 'long', 12),
    monthNamesShort: names('month', 'short', 12),
    firstDayOfWeek: options.firstDayOfWeek ?? 1,
    dateFormat: 'yy-mm-dd',
    today: labels.today,
    clear: labels.clear,
    chooseDate: labels.chooseDate,
    chooseMonth: labels.chooseMonth,
    chooseYear: labels.chooseYear,
    ...labels.navigation,
  }
}
