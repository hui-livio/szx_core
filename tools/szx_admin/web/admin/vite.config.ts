import { defineConfig, loadEnv } from 'vite'
import vue from '@vitejs/plugin-vue'
export default defineConfig(({ mode, command }) => {
  const env = loadEnv(mode, process.cwd(), '')
  return {
    base: command === 'build' ? '/admin/' : '/',
    plugins: [
      vue(),
      {
        name: 'lumina-theme-full-reload',
        handleHotUpdate(context) {
          // The preset seeds PrimeVue's shared runtime style registry.
          // Reboot that registry instead of retaining partially cached component CSS.
          if (context.file.replaceAll('\\', '/').includes('/src/ui/lumina/theme/')) {
            context.server.ws.send({ type: 'full-reload' })
            return []
          }
        },
      },
    ],
    build: {
      rolldownOptions: {
        output: {
          codeSplitting: {
            groups: [
              // Keep the renderer separate from ECharts so the lazy metrics
              // route does not combine both libraries into one oversized chunk.
              { name: 'chart-renderer', test: /[\\/]node_modules[\\/]zrender[\\/]/, priority: 20 },
              { name: 'charts', test: /[\\/]node_modules[\\/]echarts[\\/]/, priority: 10 },
            ],
          },
        },
      },
    },
    server: {
      host: 'localhost',
      port: 5173,
      proxy: {
        '/api': { target: env.ADMIN_API_TARGET || 'http://127.0.0.1:7364', changeOrigin: true },
      },
    },
  }
})
