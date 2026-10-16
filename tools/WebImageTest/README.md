# WebImageTest：HTTP + OpenCV + 原生 Web 图像查看器

独立的 C++ HTTP 服务和原生 HTML/CSS/JavaScript 页面，参考 admin 的网络接入与暗色界面，不依赖 Vue、数据库、Qt 或前端构建。复用 `szx::network::HttpServer`、`szx::common` 的文件/目录/JSON/命令行以及有界 `BlockingQueue`，不直接接入 libhv。

## 使用方式

1. 启动服务，打开 `http://127.0.0.1:7380/`。
2. 页面调用 `GET /api/images` 查询目录内的图片，默认优先选择 `moban.png`。
3. 在“选择图片”中选择文件，选择“瓦片模式”或“整图模式（非瓦片）”。
4. 点击“获取内存图片”；滚轮以指针为中心缩放、拖动平移、双指捏合、双击或 `0` 适应窗口、`1` 还原 1:1，支持 `+/-`、方向键和全屏。
5. 新增/删除文件后点击“刷新列表”。初次访问不会自动下载或解码图片。

默认目录为 `/mnt/d/share/images`。当前实际目录包含 `moban.png`（17952 × 13200，约 258 MiB）和 `1_1_01.jpg`（12122 × 10866，约 52 MiB）。图片不提交到仓库，也不通过静态目录直接暴露。

## 两种显示模式

### 瓦片模式（默认）

- 小图直接读取内存 PNG；大图首先显示最长边 2048px 的预览，放大时请求可见区域的 512px 瓦片。
- level=0 为原始像素，level=N 为 2^N 降采样；1:1 显示原始细节，而不是放大缩略图。
- WebGL2 纹理复用，Canvas2D 可降级；大图不整体上传到单个纹理，也不整体下载到浏览器。
- 瓦片请求并发≤4，CPU bitmap LRU≤64，单帧可见瓦片≤48；GPU 最多为预览加 48 个瓦片纹理。

### 整图模式（非瓦片）

- 直接从 `/api/image?name=...` 获取完整原始 PNG，**不请求 `/preview` 或 `/tile`**，只解码一次，然后缩放/平移整张图片。
- PNG≤16 MiB 时直接 GET；更大时用≤1 MiB 的 HTTP `Range` 字节区间读取，再按顺序组成完整 Blob。这是编码文件的分段传输，不是空间瓦片或图像降采样。
- WebGL2 单纹理能容纳时直接上传；原图超过 GPU 单纹理上限或上传失败时改用 Canvas2D，界面明确显示当前绘制器。
- `moban.png` 的真实整图路径已验证为 Canvas2D：17952 × 13200 原始尺寸，未请求任何预览或瓦片，支持 1:1 和平移。整图传输与解码的时间、内存明显高于瓦片模式；不承诺实体显卡 FPS/利用率。

两种模式均只在交互、图片/瓦片就绪或视口变化时合并绘制，无常驻渲染循环。画布 backing store 限制 DPR≤2、最长边≤8192、总像素≤16M；图像坐标仍为 CSS 像素，1:1 表示一个图片像素对应一个 CSS 像素。

## 服务端加载与内存策略

- **按需加载**：`/api/image/info?name=...` 触发唯一工作线程加载，HTTP IO 线程不执行大图读取、解码或预览生成。
- 每次加载读取文件一次，OpenCV `imdecode` 得到 `cv::Mat`。PNG 输入复用其编码数据，其他格式编码一次为 PNG，同时生成预览。
- 解码期间 metadata 返回 `202 {"code":1013}`；另一个名字正在解码时返回 `429 {"code":1014}`。浏览器用可取消的有限时轮询等待，最长 120 秒。
- 复用 `BlockingQueue<std::string>`，队列容量 1、解码线程 1；目录层仅保留最近一张快照。切换时释放目录缓存的旧图，已进入的响应可短暂通过 shared_ptr 持有旧快照；不同时预载整个目录。
- PNG/预览/瓦片请求必须携带同一个 `name`，不会自行切换缓存；缓存已被另一图片替换时返回 `409 {"code":1015}`，不会误返回当前另一张图片的像素。多个页面共享这一单图缓存，需要时重新点击获取。
- 浏览器切换图片/模式时取消旧请求，清理旧瓦片、GPU 纹理和 bitmap 缓存，并用请求序列号阻止迟到结果覆盖新图。上一次完整画面可保留为占位，直到新图加载成功；失败不会被当作成功切换。
- 瓦片从**内存原图 ROI** 按需编码，不重新读原文件；服务端不保留无限瓦片缓存。OpenCV 当前解码不可中途取消，关闭时等待工作线程安全退出。
- PNG/预览/瓦片响应都是二进制，不做 Base64 或 JSON 包装；HTTP response 仍有字符串拷贝，并非零拷贝服务。

## 构建

安装 OpenCV C++ 开发包的 `core`、`imgcodecs`、`imgproc`。当前系统的 package 为 `/usr/lib/cmake/opencv5/OpenCVConfig.cmake`。示例默认关闭，不影响 admin。

从仓库根目录配置独立构建目录：

```sh
cmake -S . -B build-web-image -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DSZX_COMPONENTS_BUILD_WEB_IMAGE_TEST=ON \
  -DSZX_COMPONENTS_BUILD_NETWORK=ON \
  -DSZX_COMPONENTS_BUILD_RBAC=OFF \
  -DSZX_COMPONENTS_BUILD_CAMERA=OFF \
  -DSZX_COMPONENTS_BUILD_COMPRESSION=OFF \
  -DSZX_COMPONENTS_BUILD_DMC=OFF \
  -DSZX_COMPONENTS_BUILD_FEATURE_MANAGER=OFF \
  -DSZX_COMPONENTS_BUILD_UI_TOOLKITS=OFF \
  -DSZX_COMPONENTS_BUILD_UI_TOOLKITS_APP=OFF \
  -DSZX_COMPONENTS_INSTALL=OFF \
  -DSZX_NETWORK_ENABLE_TLS=OFF \
  -DSZX_OPENCV_DIR=/usr/lib/cmake/opencv5
cmake --build build-web-image --target szx_web_image_test test_web_image --parallel 4
ctest --test-dir build-web-image -R '^test_web_image$' --output-on-failure
```

已有可正常 configure 的构建目录只需开启 `SZX_COMPONENTS_BUILD_WEB_IMAGE_TEST` / `SZX_COMPONENTS_BUILD_NETWORK`，必要时设置 `SZX_OPENCV_DIR`（包含 `OpenCVConfig.cmake` 的目录），不要覆盖无关缓存选项。

Windows 使用 x64 MSVC 开发者终端；可改用 `Ninja Multi-Config`，build/ctest 增加 `--config Release` / `-C Release`。OpenCV 与网络 DLL 需可由运行时发现，CMake 复制可识别的运行时 DLL，其他依赖将其 `bin` 加入 PATH。

## 启动与部署

```sh
./build-web-image/bin/szx_web_image_test
# 自定义图片目录与初始选择：
./build-web-image/bin/szx_web_image_test --images-dir /path/to/images --image /path/to/images/example.png --port 7380
```

| 参数           | 默认                                         | 说明                                                 |
| -------------- | -------------------------------------------- | ---------------------------------------------------- |
| `--images-dir` | `--image` 的父目录，即 `/mnt/d/share/images` | 允许列举/加载的唯一目录，只枚举直接子文件            |
| `--image`      | `/mnt/d/share/images/moban.png`              | 用文件名指定初始选择；不会允许通过 HTTP 读取其他目录 |
| `--web-root`   | 可执行文件旁 `web-image/web`                 | 静态网页目录                                         |
| `--host`       | `127.0.0.1`                                  | 默认只本机访问                                       |
| `--port`       | `7380`                                       | 0 自动分配端口，实际地址打印到控制台                 |
| `--help`       | —                                            | 参数说明                                             |

使用明确的程序路径启动，以正确推导静态资源目录。CMake 每次构建都复制网页与校准图到 `bin/web-image/`，部署时保留该目录及运行时库，无需 npm/Vite。多配置产物位于 `bin/Release/`。Ctrl+C / SIGTERM 停止。

## HTTP 接口

### 图片列表

```http
GET /api/images
```

```json
{
  "default": "moban.png",
  "items": [
    { "name": "1_1_01.jpg", "size_bytes": 54138997, "loadable": true },
    { "name": "moban.png", "size_bytes": 270799461, "loadable": true }
  ]
}
```

按名称排序，只列出 PNG/JPG/JPEG/BMP/TIF/TIFF/WebP/PPM/PGM/PBM/PNM 候选文件，扩展名不区分大小写。`loadable` 表示大小在输入上限内，不保证内容/位深可解码；真实格式在加载时校验。空目录返回空 items，默认选择为空，网页禁用获取按钮。列表刷新不做图像解码；上限 10000 个候选，超限返回错误而不隐式截断。

所有图片接口的 `name` 必须是列表中的单层文件名，使用 URLSearchParams/URL 编码处理中文、空格、`#` 等字符。省略 name 使用 `--image` 的默认文件名。

| 方法/路径                                                  | 返回与用途                                                                                                        |
| ---------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------- |
| `GET /api/image/info?name=moban.png`                       | 200 metadata；首次加载为 202，忙碌为 429，失败为具体数字码。首轮 `retry=1` 可重试上次失败，轮询不重复提交失败任务 |
| `GET /api/image?name=moban.png`                            | 已缓存 PNG；≤16 MiB 可直接 GET，更大时无 Range 返回 413/code=1017，须使用下一行                                   |
| `GET /api/image?name=moban.png` + `Range: bytes=0-1048575` | 206 原始 PNG 字节范围，单次≤1 MiB；校验 Content-Range 和长度；不支持多区间/后缀区间，非法范围返回 416             |
| `GET /api/image/preview?name=moban.png`                    | 启动该快照时生成的最长边 2048px PNG                                                                               |
| `GET /api/image/tile/0/35/25?name=moban.png`               | 512px 瓦片或实际边缘大小；例中右下角 32 × 400；非法索引返回 400                                                   |
| `GET /api/health`                                          | ready，表示 HTTP 服务就绪，不表示已经解码图片                                                                     |
| `GET /`、`GET /app.js` 等                                  | 静态 HTML/CSS/JS，无 SPA 回退，不允许目录遍历                                                                     |

metadata 包含 `width`、`height`、`channels`、`depth_bits`、`encoded_bytes`、`content_type`、`preview_width`、`preview_height`、`tile_size`、`max_level`、`tiled`。图片快照编码最多 512 MiB/256M 原始像素；预览/瓦片不将整个原图传到浏览器。响应 `Cache-Control: no-store`，支持 HEAD，Range HEAD 不传输 body。

## 安全和限制

- 本机演示无认证/TLS/限速，**不能直接暴露到公网**。开放局域网需额外的访问控制、防火墙和反向代理。
- 目录根路径由管理员指定，文件名禁止 `/`、`\\`、冒号、控制字符及目录穿越；跳过符号链接并验证 canonical parent，静态目录同样复用 network 的安全边界。要求不可信进程不能修改目录树，未宣称能防御恶意并发替换的 TOCTOU。
- 使用可信本地图片。文件≤512 MiB、解码≤256M 像素的后置检查不是恶意压缩图片的内存沙箱；必要时进程启动前设置 OpenCV `OPENCV_IO_MAX_IMAGE_PIXELS` 等解码器限制。
- 支持 8-bit gray/BGR/BGRA，16-bit/浮点拒绝并返回 1004；不隐式截断、不做 EXIF/ICC 校准或 AOI 检测。
- 单张 moban 原始矩阵约 678 MiB，编码约 258 MiB；保留完整快照本身就需要接近 1 GiB 内存。整图模式还需浏览器保存完整编码/解码结果，内存明显高于瓦片模式。16 个连接上限不是硬内存总量限制。
- 缓存中的文件不会每请求重新 stat/解码；更新相同文件后可切换到另一图片再切回，或重启服务。图片加载失败可修复文件后再次点击获取。
- 所有异常在应用/线程边界包含，外部只见应用所属数字码，无 OpenCV 异常文本或服务器绝对路径。namespace 为内部 `SZX::WEBIMAGE::inline v1`，不发布 SDK/ABI，也无子模块 offset 聚合。

错误码：1001 配置、1002 读取、1003 解码、1004 像素格式、1005 大小、1006 编码、1007 内存、1008 未预期；1010 目录不可用、1011 名称无效、1012 文件不存在、1013 正在加载、1014 加载器忙、1015 快照未缓存、1016 列表超限、1017 整图需 Range；1101 网络配置、1102 监听失败。

## 验证

C++ 测试覆盖像素保真、PNG/预览/瓦片、边缘坐标、源文件删除后内存服务、并发读取、HEAD/Range、目录遍历/链接拒绝、空目录、目录列表、单图缓存隔离和失败重试。

```sh
cd tools/WebImageTest
npm test                    # Node 数学/资源/URL/轮询/范围组装测试，无运行时 npm 依赖
npm run test:browser         # 前端夹具，不能代替 C++ 实测
npm run test:catalog         # 自动启动真实 C++ 服务，临时多图片目录与选择/模式/竞态/刷新回归
npm run test:large           # 连接 7380，验证实际 PNG/JPG 瓦片模式
npm run test:full-large      # 连接 7380，验证完整 moban 非瓦片模式；会完整下载 258 MiB，内存较高
```

浏览器测试仅开发时复用 admin 安装的 Playwright/Chromium；生产页无 JS 包依赖。可用 `WEB_IMAGE_URL` 指定服务、`WEB_IMAGE_BINARY` 指定 catalog 测试的程序。颜色/方向校准图的完整 live 测试需另起 `--image assets/sample.ppm --port 7381`，再设置 `WEB_IMAGE_URL=http://127.0.0.1:7381 npm run test:browser`。

截图/结果位于 `test-results/`（Git 忽略）。测试使用 headless Chromium/SwiftShader，仅验证功能、资源边界和空闲行为，不是实体 GPU 性能认证。目录/模式回归包含中文文件名、迟到请求、不串图、坏文件修复重试、刷新和窄屏；实际 moban 的整图模式确认无预览/瓦片请求。

```sh
# 仓库根目录
python scripts/format_cpp.py tools/WebImageTest
# 示例目录，复用 admin 的 Prettier
npm run format
npm run format:check
```

`assets/sample.ppm` 是仓库自行生成的 RGB 校准图，不含第三方素材。C++ 目录枚举需要明确区分 IO 失败与空目录，因此使用带 error_code 的标准库迭代器；FolderHelper 的原枚举合同会返回部分/空结果，不适合作为该 HTTP 合同。无需通用任务 ID/任务历史，使用有界 BlockingQueue + 单 worker，而非另加一套队列或把快照额外留在 WorkQueueService 历史记录中。
