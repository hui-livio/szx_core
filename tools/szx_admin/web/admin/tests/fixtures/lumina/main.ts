import { createApp } from 'vue'
import { createLuminaUi, createLuminaCalendarLocale } from '../../../src/ui/lumina'
import '../../../src/ui/lumina/styles.css'
import Showcase from './Showcase.vue'

createApp(Showcase)
  .use(
    createLuminaUi({
      locale: createLuminaCalendarLocale(
        'en-US',
        {
          today: 'Today',
          clear: 'Clear',
          chooseDate: 'Choose date',
          chooseMonth: 'Choose month',
          chooseYear: 'Choose year',
          navigation: { nextSecond: 'Next second', prevSecond: 'Previous second' },
        },
        { firstDayOfWeek: 0 },
      ),
    }),
  )
  .mount('#app')
