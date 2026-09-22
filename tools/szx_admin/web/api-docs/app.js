/* global SwaggerUIBundle */
'use strict'

async function initializeDocs() {
  const status = document.getElementById('load-status')
  try {
    const response = await fetch('./openapi.json', { cache: 'no-store' })
    if (!response.ok) throw new Error(`HTTP ${response.status}`)
    const spec = await response.json()
    // The configured API prefix, including deployments using '/', owns this page.
    spec.servers = [{ url: location.pathname.replace(/\/$/, '') || '/', description: '当前服务' }]
    SwaggerUIBundle({
      spec,
      dom_id: '#swagger-ui',
      deepLinking: true,
      filter: true,
      docExpansion: 'list',
      defaultModelsExpandDepth: 0,
      displayRequestDuration: true,
      persistAuthorization: false,
      withCredentials: true,
      validatorUrl: null,
      queryConfigEnabled: false,
      supportedSubmitMethods: ['get', 'post', 'put', 'patch'],
      presets: [SwaggerUIBundle.presets.apis],
      layout: 'BaseLayout',
    })
    status.hidden = true
  } catch (error) {
    status.textContent = `接口文档加载失败（${error.message}）。请检查 openapi.json 和文档资源是否已部署，然后刷新页面。`
  }
}

initializeDocs()
