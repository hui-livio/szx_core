# WebImageTest：独立加载 + BGRA8 原始像素 HTTP 查看器

C++ / OpenCV 服务与原生 HTML/CSS/JavaScript 页面，不使用 Vue、数据库或 Qt。复用 `szx::network::HttpServer`、`szx::common` 的文件/路径/JSON/UUID/有界队列。图片目录默认 `/mnt/d/share/images`，优先选择 `moban.png`。

## 操作分为两步

1. 选择目录图片，点击 **“后端加载并解码 BGRA8”**。
   - 仅 `POST /api/image/load?name=...` 提交后台加载。
   - 工作线程读取源文件，用 OpenCV 解码，并转换为连续的 `CV_8UC4`（BGRA8），准备 raw 预览与 metadata 后，才发布 `state=ready`。
   - BGR8 转 BGRA8，gray8 转 BGRA8，原有 BGRA8 保留 Alpha；没有 Alpha 的来源补 255。16-bit/float 返回格式错误，不隐式截断。
   - 浏览器以 GET `/api/image/load` 查询状态。**GET 不读盘、不解码、不提交任务**。
2. 加载完成后点击 **“获取内存图片”**。
   - 从已解码快照请求 BGRA8 原始字节，直接作为 `ArrayBuffer` / `Uint8Array` 使用。
   - **不执行 PNG/JPEG 编码，不传 Base64，不在浏览器执行压缩图解码**。
   - 原图、预览、瓦片全部是 `application/octet-stream` / BGRA8，不混用压缩图格式。

“刷新列表”查询目录候选图片，不解码。切换文件取消旧加载/传输和瓦片缓存，必须重新完成后端加载；只切换显示模式可复用已就绪的后端快照。加载失败可修复文件后重新点击后端加载。

## 两种显示模式

### 瓦片模式（默认）

小图直接获取 BGRA8 原图，大图先获取最长边 2048px 的 BGRA8 预览，放大后获取原图可见区域的 BGRA8 瓦片。level=0 为原始像素，level=N 为 2^N 降采样，边缘按实际尺寸返回。预览和瓦片是缩放/ROI 像素，**不是 PNG 编码**。

瓦片 HTTP 并发≤4，raw buffer LRU≤64，可见瓦片≤48。WebGL2 复用已上传纹理，切换后删除旧 GPU 纹理并移除 JS 缓冲引用。ROI 可能保留原图 step，因此服务端逐行打包，只返回 `width*4` 个有效字节，绝不发送相邻区域或行间隙。

### 整图模式（非瓦片）

从 `/api/image?name=...&id=...` 读取完整 BGRA8 原始矩阵，不请求预览或空间瓦片。≤16 MiB 直接读取，更大时用≤1 MiB 的 HTTP Range 字节段直接填入**一个最终 Uint8Array**，不先拼接 Blob 再解码，也不再分配一份完整的格式转换副本。

整图依然会占用很高的带宽和内存。例如 moban 为 17952 × 13200：

```text
stride     = 17952 * 4 = 71808 bytes
byte_length = 71808 * 13200 = 947865600 bytes ≈ 904 MiB
```

完整 BGRA8 大于原始 PNG 文件约 258 MiB 的编码大小。改变传输格式不会降低原始纹理尺寸，也不等同于零拷贝。

## 绘制与交互

- **WebGL2**：`texImage2D(RGBA8, RGBA, UNSIGNED_BYTE, raw Uint8Array)` 上传 BGRA 字节，fragment shader 使用 `.bgra` 交换红蓝，按 straight alpha 合成。已就绪区域缩放/平移只更新 uniform，不重新解码/上传。
- **Canvas2D 降级**：WebGL 不可用、纹理尺寸超限或上传失败时启用。直接从 BGRA buffer 按可见屏幕范围采样/交换通道，写入 viewport-sized `ImageData`；不会创建原图大小的 RGBA canvas 或第二份整图转换缓冲。采用最近邻采样，属于 CPU 兼容路径，不能宣称与 GPU 相同性能。
- 只有交互、raw 图像/瓦片就绪或窗口变化时合并绘制，没有常驻 rAF 循环；隐藏页面不绘制。
- 画布 backing store 限制 DPR≤2、边长≤8192、总像素≤16M；不影响原图坐标与可见瓦片选择。
- 滚轮以指针为中心缩放、拖动平移、双指缩放、双击或 `0` 适应窗口、`1` 为一个图片像素对应一个 CSS 像素，另有 `+/-`、方向键、全屏。
- GPU context 恢复时重新上传保留的 raw buffer，不请求后端加载、不调用 `createImageBitmap`。

## API

### `GET /api/images`

列举目录直接子文件，按名称排序。只列出 PNG/JPG/JPEG/BMP/TIF/TIFF/WebP/PPM/PGM/PBM/PNM 扩展名，跳过链接/目录；`loadable` 只表示源文件大小合规，不保证内容/位深有效。

```json
{
  "default": "moban.png",
  "items": [
    { "name": "1_1_01.jpg", "size_bytes": 54138997, "loadable": true },
    { "name": "moban.png", "size_bytes": 270799461, "loadable": true }
  ]
}
```

`name` 使用 URL 编码，不能包含目录路径；省略时使用默认文件名。空目录 items 为空，页面禁用加载。最多 10000 个候选文件，超出返回错误而非隐式截断。

### 后端加载命令与状态

```http
POST /api/image/load?name=moban.png
GET /api/image/load?name=moban.png
```

- 未加载状态 GET 返回 `409 {"code":1015}`，不会启动工作。
- POST 提交后通常为 `202 {"code":1013}`；已加载同一快照时为 200。
- GET 状态：正在处理为 202、失败为具体数字码、就绪为 200。
- 单 loader 正在加载另一名称时 POST 为 `429 {"code":1014}`，浏览器有界等待，状态轮询最长 120 秒。
- 仅当解码、BGRA8 转换、连续布局和 raw 预览全部就绪才返回如下 ready metadata：

```json
{
  "state": "ready",
  "id": "snapshot-uuid",
  "width": 17952,
  "height": 13200,
  "format": "BGRA8",
  "channels": 4,
  "depth_bits": 8,
  "stride": 71808,
  "byte_length": 947865600,
  "origin": "top-left",
  "alpha": "straight",
  "content_type": "application/octet-stream",
  "preview_width": 2048,
  "preview_height": 1506,
  "preview_byte_length": 12337152,
  "tile_size": 512,
  "max_level": 6,
  "tiled": true
}
```

`GET /api/image/info?name=...` 也只读该 metadata，不触发加载。快照 id 来自 Uuid；每次实际加载生成不同身份，数据请求带 `id` 后必须与快照匹配，防止同名文件重载或其他页面切图时字节段混合。

### BGRA8 数据

| 路径                                                         | 内容                                          |
| ------------------------------------------------------------ | --------------------------------------------- |
| `GET /api/image?name=...&id=...`                             | 完整 BGRA8 像素，`byte_length=width*height*4` |
| `GET /api/image/preview?name=...&id=...`                     | 缩小的连续 BGRA8 像素                         |
| `GET /api/image/tile/{level}/{column}/{row}?name=...&id=...` | 打包的 BGRA8 ROI，实际宽高由响应头给出        |

共同响应头：

```http
Content-Type: application/octet-stream
Cache-Control: no-store
Accept-Ranges: bytes
X-Pixel-Format: BGRA8
X-Image-Width: 17952
X-Image-Height: 13200
X-Image-Stride: 71808
X-Image-Byte-Length: 947865600
X-Image-Origin: top-left
X-Image-Alpha: straight
X-Image-Id: snapshot-uuid
```

每个像素按 **B、G、R、A 四个 uint8 字节** 排列，从左到右、从上到下，无行 padding；这是 raw 像素协议，不是 BMP/PNG 文件，不能作为 `<img src>` 直接显示。宽高、stride、byte_length、id、格式和实际 body 长度都会在前端校验。

大图无 Range 的 GET 为 `413/code=1017`。携带 `Range: bytes=0-1048575` 返回 206 + `Content-Range: bytes 0-1048575/947865600`；单段≤1 MiB，只支持明确的单区间起止，不支持后缀、多段、开放终点，非法为416。分段边界不要求落在行边界，按**字节偏移**填充最终 buffer 即可。Range 响应的像素描述头仍为整个矩阵信息，body 只是其中字节段。

数据接口只读取缓存，未加载、仍加载中、失败、已被其他图片替换或 id 不匹配均不会隐式重新加载，也不会返回另一图片的数据。HEAD 保留描述信息但不发送 body。`GET /api/health` 只表示 HTTP 服务就绪。

## 构建与启动

OpenCV C++ 开发包需要 `core` / `imgcodecs` / `imgproc`。本机通过 `/usr/lib/cmake/opencv5/OpenCVConfig.cmake` 验证。示例在独立构建时默认开启，作为子项目时默认关闭；OpenCV 仅链接到图片示例目标。可设置 `SZX_COMPONENTS_BUILD_WEB_IMAGE_TEST=OFF` 关闭示例；关闭网络组件时也需关闭此示例。

```sh
# 仓库根目录
cmake -S . -B build-web-image -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
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
./build-web-image/bin/szx_web_image_test
# http://127.0.0.1:7380/
```

全新独立构建无需额外开启示例或网络选项。已有构建目录会保留 CMake 缓存中的旧值，如原先为 OFF，需重新配置并显式传入 `-DSZX_COMPONENTS_BUILD_WEB_IMAGE_TEST=ON -DSZX_COMPONENTS_BUILD_NETWORK=ON`；必要时设置 `SZX_OPENCV_DIR`（包含 OpenCVConfig.cmake 的目录）。Windows 使用 x64 MSVC 开发者终端，Multi-Config build/ctest 增加 `--config Release` / `-C Release`；对应 DLL 需可被运行时找到。

| 参数           | 默认                                     | 说明                        |
| -------------- | ---------------------------------------- | --------------------------- |
| `--images-dir` | `--image` 的父目录 `/mnt/d/share/images` | 唯一允许列举/加载的图片目录 |
| `--image`      | `/mnt/d/share/images/moban.png`          | 初始选中的文件；不自动解码  |
| `--web-root`   | 可执行文件旁 `web-image/web`             | 静态资源                    |
| `--host`       | `127.0.0.1`                              | 只本机访问                  |
| `--port`       | `7380`                                   | 0 自动分配                  |

使用明确程序路径启动以正确推导资源目录；CMake 每次构建复制网页及校准素材到程序旁 `web-image/`，部署无需 npm/Vite。Ctrl+C/SIGTERM 退出，安全等待不能中途取消的 OpenCV 解码。

## 资源、安全与错误契约

- 只有一个有界队列和解码工作线程，最多缓存最近一张 BGRA8 快照。旧快照可短暂由进行中的响应持有，不预载目录全部图片。
- 加载过程释放压缩输入与原始非 BGRA decode buffer，**就绪快照不保留 PNG/JPEG 编码副本**，只保留 BGRA8 原图、raw 预览和 metadata；小图预览可共享原图。
- 源文件≤512 MiB，解码原图≤256M 像素，BGRA8 原图≤1 GiB。解码后的检查不是恶意压缩图片沙箱；仅使用可信本地输入，必要时启动前设置 OpenCV 解码器像素限制。16个连接上限也不是总内存限额。
- moban 就绪原图约904 MiB；整图模式还需浏览器约904 MiB raw buffer，网络有分段拷贝、GPU有上传，不是零拷贝。Canvas降级不额外创建同尺寸 RGBA 全图缓冲，但每次重绘仍有可见像素 CPU 处理。
- 目录禁止路径穿越、反斜杠、冒号、控制字符，排除符号链接并检查 canonical parent；要求不可信进程不能修改目录树，不宣称消除恶意并发替换 TOCTOU。
- 缓存同一图片时再次 POST 复用已就绪 BGRA8；同名源文件修改后可切到另一图片再切回或重启。坏文件修复后需再次显式 POST，GET 状态不会自动重试。
- 默认为本机演示，无认证/TLS/限速，**不要直接暴露到公网**。CSP/nosniff/referrer policy 不替代访问控制。
- 应用/工作线程边界包含异常，只暴露应用所属数字码，不返回 OpenCV 异常文本和服务器绝对路径。内部 API 为 `SZX::WEBIMAGE::inline v1`，不安装公共 SDK，无 offset 聚合。

错误码：1001配置，1002读取，1003解码，1004像素类型，1005大小，1006像素处理，1007内存，1008未预期；1010目录，1011文件名，1012文件不存在，1013加载中，1014忙，1015未加载，1016目录超限，1017需Range，1018快照身份不匹配；1101网络配置，1102监听。

## 验证

```sh
ctest --test-dir build-web-image -R '^(test_web_image|test_modules_network)$' --output-on-failure
cd tools/WebImageTest
npm test
npm run test:browser       # BGRA8 前端夹具，不代替 C++ 实测
npm run test:catalog       # 自动启动真实 C++；临时目录、多图片、加载/获取分离和模式切换
npm run test:large         # 实际 moban/JPG 的 BGRA8 预览和瓦片，连接7380
npm run test:full-large    # 实际 moban 全部947865600字节，连接7380，资源开销较高
```

校准图真实服务：另起 `--image tools/WebImageTest/assets/sample.ppm --port 7381`，再在示例目录运行 `WEB_IMAGE_URL=http://127.0.0.1:7381 npm run test:browser`。测试复用 admin 的 Playwright/Chromium，仅开发测试需要 Node 工具，生产页无包依赖。支持 `WEB_IMAGE_BINARY` / `WEB_IMAGE_URL`。截图/报告在被 Git 忽略的 `test-results/`。

测试覆盖 gray/BGR/BGRA → 连续BGRA8、原始字节/Alpha/通道顺序、非连续ROI逐行打包、边缘瓦片、GET不加载、POST/状态分离、失败显式重试、身份校验、Range、列表/目录安全、并发/旧请求隔离。浏览器禁止应用调用 createImageBitmap，校验 WebGL/Canvas 通道、方向、Alpha、纹理复用、空闲不绘制、上下文恢复、双指及窄屏。headless SwiftShader 验证功能和资源边界，**不是实体GPU性能认证**。

```sh
# 根目录
python scripts/format_cpp.py tools/WebImageTest
# 示例目录
npm run format
npm run format:check
```

`assets/sample.ppm` 是仓库生成的RGB校准图。目录枚举需要区分IO失败与空目录，使用带error_code的标准迭代器；FolderHelper枚举的部分/空结果语义不适合该HTTP合同。基础文件/JSON/UUID/队列仍复用 common，不引入第二套工具库。
