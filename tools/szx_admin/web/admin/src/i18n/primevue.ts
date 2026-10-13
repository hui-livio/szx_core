import type { PrimeVueLocaleOptions } from '@primevue/core/config'
import { translate, type SupportedLocale } from './index'

export function datePickerLocale(locale: SupportedLocale): Partial<PrimeVueLocaleOptions> {
  const names = (field: 'weekday' | 'month', width: 'long' | 'short' | 'narrow', count: number) => {
    const formatter = new Intl.DateTimeFormat(locale, { [field]: width, timeZone: 'UTC' })
    return Array.from({ length: count }, (_, index) =>
      formatter.format(
        new Date(
          Date.UTC(2023, field === 'month' ? index : 0, field === 'weekday' ? index + 1 : 1),
        ),
      ),
    )
  }
  const navigation = Object.fromEntries(
    ['Month', 'Year', 'Decade', 'Hour', 'Minute', 'Second'].flatMap((unit) => [
      [
        `prev${unit}`,
        translate('datePicker.previous', { unit: translate(`datePicker.${unit.toLowerCase()}`) }),
      ],
      [
        `next${unit}`,
        translate('datePicker.next', { unit: translate(`datePicker.${unit.toLowerCase()}`) }),
      ],
    ]),
  )
  return {
    dayNames: names('weekday', 'long', 7),
    dayNamesShort: names('weekday', 'short', 7),
    dayNamesMin: names('weekday', 'narrow', 7),
    monthNames: names('month', 'long', 12),
    monthNamesShort: names('month', 'short', 12),
    firstDayOfWeek: locale === 'en-US' ? 0 : 1,
    dateFormat: 'yy-mm-dd',
    today: translate('datePicker.today'),
    clear: translate('datePicker.clear'),
    chooseDate: translate('datePicker.chooseDate'),
    chooseMonth: translate('datePicker.chooseMonth'),
    chooseYear: translate('datePicker.chooseYear'),
    ...navigation,
  }
}
