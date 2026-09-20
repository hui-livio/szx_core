import { defineConfig, loadEnv } from 'vite'
import vue from '@vitejs/plugin-vue'
export default defineConfig(({ mode, command }) => {
  const env = loadEnv(mode, process.cwd(), '')
  return {
    base: command === 'build' ? '/admin/' : '/',
    plugins: [vue()],
    server: {
      host: 'localhost',
      port: 5173,
      proxy: {
        '/api': { target: env.ADMIN_API_TARGET || 'http://127.0.0.1:7364', changeOrigin: true },
      },
    },
  }
})
