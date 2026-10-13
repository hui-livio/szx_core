import { createLuminaCalendarLocale, type LuminaLocale } from '../ui/lumina'
import { translate, type SupportedLocale } from './index'

export function datePickerLocale(locale: SupportedLocale): LuminaLocale {
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
  return createLuminaCalendarLocale(
    locale,
    {
      today: translate('datePicker.today'),
      clear: translate('datePicker.clear'),
      chooseDate: translate('datePicker.chooseDate'),
      chooseMonth: translate('datePicker.chooseMonth'),
      chooseYear: translate('datePicker.chooseYear'),
      navigation,
    },
    { firstDayOfWeek: locale === 'en-US' ? 0 : 1 },
  )
}
