import { createApp } from 'vue'
import { createLuminaUi } from '../../../src/ui/lumina'
import '../../../src/ui/lumina/styles.css'
import Updates from './Updates.vue'

createApp(Updates).use(createLuminaUi()).mount('#app')
