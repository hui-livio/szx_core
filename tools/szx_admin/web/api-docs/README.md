# Admin API 文档资源

服务在 `${server.apiPrefix}/` 提供 Swagger UI，在 `${server.apiPrefix}/openapi.json` 提供 OpenAPI 3.0.3。默认入口 `/api/v1` 跳转到 `/api/v1/`。页面无 CDN、远程校验器或遥测依赖，资源由当前服务加载。

- `index.html` / `style.css`：页面展示。
- `app.js`：加载 OpenAPI、匹配实际 API 前缀和初始化 Swagger UI。
- `openapi.json`：现有 26 条路径、36 个操作的参数、响应、鉴权和示例；修改接口时同步更新此文件与 Markdown 接口文档。
- `vendor/`：未经修改的 `swagger-ui-dist@5.33.0` 浏览器发行文件、Apache-2.0 LICENSE 和 NOTICE。

vendor 来源为 npm 包 `swagger-ui-dist@5.33.0`，包完整性为：

```text
sha512-wpdK+m6BU5yj6pmUdMskZVTSWYG4DLglAx3sIhylloY37i8O37IrH+YEpqdXNfpaTGxILRBFzUqLF2jKqbfI7A==
```

更新依赖时用 `npm pack swagger-ui-dist@<固定版本>` 获取发行包，只替换 `swagger-ui-bundle.js`、`swagger-ui.css`、`LICENSE`、`NOTICE`，同步记录版本和 integrity。不要格式化或手工修改 vendor 文件。

构建 `szx_admin_service` 时，`szx_admin_service_web_assets` 会复制整个 `web` 到可执行文件旁，即使 exe 无需重新链接也会更新文档。自定义 `staticRoot` 的部署需复制本目录到 `<staticRoot>/api-docs`。首次创建目录后重启服务。

Authorize 填写实际登录响应的 access_token；刷新/登出使用浏览器自动管理的 Cookie 和手动填写的最新 X-CSRF-Token。页面不自动刷新、不持久化授权信息；Try it out 执行真实请求。填写密码字段时提交 SHA-256 摘要，不是明文。
