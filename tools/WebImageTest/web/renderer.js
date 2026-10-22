import { backingSize } from './viewport.js'

const VERTEX = `#version 300 es
in vec2 a_position;
uniform vec2 u_viewport;
uniform vec2 u_image;
uniform vec2 u_center;
uniform float u_zoom;
out vec2 v_uv;
void main() {
  vec2 pixel = (a_position - 0.5) * u_image * u_zoom + u_center;
  gl_Position = vec4(pixel.x / u_viewport.x * 2.0 - 1.0, 1.0 - pixel.y / u_viewport.y * 2.0, 0.0, 1.0);
  v_uv = a_position;
}`
const FRAGMENT = `#version 300 es
precision highp float;
in vec2 v_uv;
uniform sampler2D u_texture;
out vec4 outColor;
void main() {
  vec4 sampleColor = texture(u_texture, v_uv).bgra;
  // Composite once against the canvas background: a translucent detail tile must
  // replace, not blend a second time over, the already composited preview.
  outColor = vec4(mix(vec3(11.0/255.0, 14.0/255.0, 25.0/255.0), sampleColor.rgb, sampleColor.a), 1.0);
}`

function shader(gl, kind, text) {
  const result = gl.createShader(kind)
  if (!result) throw new Error('无法分配着色器。')
  gl.shaderSource(result, text)
  gl.compileShader(result)
  if (!gl.getShaderParameter(result, gl.COMPILE_STATUS)) {
    gl.deleteShader(result)
    throw new Error('无法编译图像着色器。')
  }
  return result
}

class WebGLRenderer {
  name = 'WebGL2 · GPU texture'
  image = null
  tiles = new Map()
  constructor(canvas, gl) {
    this.canvas = canvas
    this.gl = gl
    try {
      this.initialize()
    } catch (error) {
      this.dispose()
      throw error
    }
  }

  initialize() {
    const gl = this.gl
    this.limit = gl.getParameter(gl.MAX_TEXTURE_SIZE)
    let vertex, fragment
    try {
      vertex = shader(gl, gl.VERTEX_SHADER, VERTEX)
      fragment = shader(gl, gl.FRAGMENT_SHADER, FRAGMENT)
      this.program = gl.createProgram()
      if (!this.program) throw new Error('无法分配 GPU 程序。')
      gl.attachShader(this.program, vertex)
      gl.attachShader(this.program, fragment)
      gl.linkProgram(this.program)
      if (!gl.getProgramParameter(this.program, gl.LINK_STATUS))
        throw new Error('GPU 程序链接失败。')
    } finally {
      if (vertex) gl.deleteShader(vertex)
      if (fragment) gl.deleteShader(fragment)
    }
    this.buffer = gl.createBuffer()
    this.vao = gl.createVertexArray()
    if (!this.buffer || !this.vao) throw new Error('无法分配 GPU 缓冲区。')
    gl.bindVertexArray(this.vao)
    gl.bindBuffer(gl.ARRAY_BUFFER, this.buffer)
    gl.bufferData(
      gl.ARRAY_BUFFER,
      new Float32Array([0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 1]),
      gl.STATIC_DRAW,
    )
    const position = gl.getAttribLocation(this.program, 'a_position')
    gl.enableVertexAttribArray(position)
    gl.vertexAttribPointer(position, 2, gl.FLOAT, false, 0, 0)
    this.uniforms = Object.fromEntries(
      ['viewport', 'image', 'center', 'zoom', 'texture'].map((name) => [
        name,
        gl.getUniformLocation(this.program, `u_${name}`),
      ]),
    )
    gl.useProgram(this.program)
    gl.uniform1i(this.uniforms.texture, 0)
    gl.disable(gl.BLEND)
    gl.clearColor(11 / 255, 14 / 255, 25 / 255, 1)
  }

  setImage(image, reuseTexture = false) {
    const gl = this.gl
    if (image.width > this.limit || image.height > this.limit)
      throw new Error(`图片超过 GPU 纹理上限 ${this.limit}。`)
    if (
      reuseTexture &&
      this.texture &&
      this.image?.width === image.width &&
      this.image?.height === image.height
    ) {
      if (image.format !== 'BGRA8' || image.data.byteLength !== image.width * image.height * 4)
        throw new Error('GPU 输入不是连续 BGRA8 数据。')
      gl.activeTexture(gl.TEXTURE0)
      gl.bindTexture(gl.TEXTURE_2D, this.texture)
      gl.pixelStorei(gl.UNPACK_ALIGNMENT, 4)
      gl.texSubImage2D(
        gl.TEXTURE_2D,
        0,
        0,
        0,
        image.width,
        image.height,
        gl.RGBA,
        gl.UNSIGNED_BYTE,
        image.data,
      )
      if (gl.getError() !== gl.NO_ERROR) throw new Error('GPU 纹理更新失败。')
    } else {
      const next = this.upload(image)
      if (this.texture) gl.deleteTexture(this.texture)
      this.texture = next
    }
    this.image = image
    for (const tile of this.tiles.values()) gl.deleteTexture(tile.texture)
    this.tiles.clear()
  }

  upload(image) {
    const gl = this.gl
    const texture = gl.createTexture()
    if (!texture) throw new Error('无法分配图像纹理。')
    try {
      gl.activeTexture(gl.TEXTURE0)
      gl.bindTexture(gl.TEXTURE_2D, texture)
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR)
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR)
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE)
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE)
      if (image.format !== 'BGRA8' || image.data.byteLength !== image.width * image.height * 4)
        throw new Error('GPU 输入不是连续 BGRA8 数据。')
      gl.pixelStorei(gl.UNPACK_ALIGNMENT, 4)
      gl.texImage2D(
        gl.TEXTURE_2D,
        0,
        gl.RGBA8,
        image.width,
        image.height,
        0,
        gl.RGBA,
        gl.UNSIGNED_BYTE,
        image.data,
      )
      if (gl.getError() !== gl.NO_ERROR) throw new Error('GPU 纹理上传失败。')
      return texture
    } catch (error) {
      gl.deleteTexture(texture)
      throw error
    }
  }

  setTiles(tiles) {
    const wanted = new Set(tiles.map((tile) => tile.key))
    for (const [key, tile] of this.tiles) {
      if (!wanted.has(key)) {
        this.gl.deleteTexture(tile.texture)
        this.tiles.delete(key)
      }
    }
    for (const tile of tiles) {
      if (!this.tiles.has(tile.key))
        this.tiles.set(tile.key, { ...tile, texture: this.upload(tile.image) })
    }
  }

  draw(view) {
    const gl = this.gl
    const size = backingSize(view.width, view.height, window.devicePixelRatio)
    if (this.canvas.width !== size.width || this.canvas.height !== size.height) {
      this.canvas.width = size.width
      this.canvas.height = size.height
    }
    gl.viewport(0, 0, this.canvas.width, this.canvas.height)
    gl.clear(gl.COLOR_BUFFER_BIT)
    if (!this.image || gl.isContextLost()) return
    gl.useProgram(this.program)
    gl.bindVertexArray(this.vao)
    gl.activeTexture(gl.TEXTURE0)
    gl.bindTexture(gl.TEXTURE_2D, this.texture)
    gl.uniform2f(this.uniforms.viewport, view.width, view.height)
    gl.uniform2f(this.uniforms.image, view.imageWidth, view.imageHeight)
    gl.uniform2f(this.uniforms.center, view.x, view.y)
    gl.uniform1f(this.uniforms.zoom, view.zoom)
    gl.drawArrays(gl.TRIANGLES, 0, 6)
    // Original-pixel bounds preserve 1:1 detail without a giant source texture.
    for (const tile of this.tiles.values()) {
      gl.bindTexture(gl.TEXTURE_2D, tile.texture)
      gl.uniform2f(this.uniforms.image, tile.width, tile.height)
      gl.uniform2f(
        this.uniforms.center,
        view.x + (tile.x + tile.width / 2 - view.imageWidth / 2) * view.zoom,
        view.y + (tile.y + tile.height / 2 - view.imageHeight / 2) * view.zoom,
      )
      gl.drawArrays(gl.TRIANGLES, 0, 6)
    }
  }

  dispose() {
    const gl = this.gl
    if (this.texture) gl.deleteTexture(this.texture)
    for (const tile of this.tiles.values()) gl.deleteTexture(tile.texture)
    this.tiles.clear()
    if (this.buffer) gl.deleteBuffer(this.buffer)
    if (this.vao) gl.deleteVertexArray(this.vao)
    if (this.program) gl.deleteProgram(this.program)
    this.image = null
    // Release the old framebuffer/context promptly when replacing a WebGL canvas.
    gl.getExtension('WEBGL_lose_context')?.loseContext()
  }
}

class CanvasRenderer {
  tiles = []
  image = null
  output = null
  name = 'Canvas2D · BGRA8 CPU fallback'
  constructor(canvas) {
    this.canvas = canvas
    this.context = canvas.getContext('2d', { alpha: false })
    if (!this.context) throw new Error('浏览器无法创建图像画布。')
  }
  setImage(image) {
    this.image = image
    this.tiles = []
  }
  setTiles(tiles) {
    this.tiles = tiles
  }
  draw(view) {
    const size = backingSize(view.width, view.height, window.devicePixelRatio)
    if (this.canvas.width !== size.width || this.canvas.height !== size.height) {
      this.canvas.width = size.width
      this.canvas.height = size.height
      this.output = null
    }
    const ctx = this.context
    if (!this.output) this.output = ctx.createImageData(size.width, size.height)
    const target = this.output.data
    for (let i = 0; i < target.length; i += 4) {
      target[i] = 11
      target[i + 1] = 14
      target[i + 2] = 25
      target[i + 3] = 255
    }
    // Bounded viewport-sized output: no giant temporary canvas or whole-image RGBA conversion.
    const paint = (source, x, y, w, h) => {
      const sx = size.width / view.width,
        sy = size.height / view.height
      const left = Math.max(0, Math.floor(x * sx)),
        top = Math.max(0, Math.floor(y * sy))
      const right = Math.min(size.width, Math.ceil((x + w) * sx)),
        bottom = Math.min(size.height, Math.ceil((y + h) * sy))
      const fx = source.width / (w * sx),
        fy = source.height / (h * sy)
      for (let py = top; py < bottom; py++) {
        const row =
          Math.min(source.height - 1, Math.max(0, Math.floor((py + 0.5 - y * sy) * fy))) *
          source.width
        for (let px = left; px < right; px++) {
          const column = Math.min(
            source.width - 1,
            Math.max(0, Math.floor((px + 0.5 - x * sx) * fx)),
          )
          const input = (row + column) * 4,
            output = (py * size.width + px) * 4
          const alpha = source.data[input + 3] / 255,
            inverse = 1 - alpha
          target[output] = source.data[input + 2] * alpha + 11 * inverse
          target[output + 1] = source.data[input + 1] * alpha + 14 * inverse
          target[output + 2] = source.data[input] * alpha + 25 * inverse
        }
      }
    }
    if (this.image)
      paint(
        this.image,
        view.x - (view.imageWidth * view.zoom) / 2,
        view.y - (view.imageHeight * view.zoom) / 2,
        view.imageWidth * view.zoom,
        view.imageHeight * view.zoom,
      )
    for (const tile of this.tiles)
      paint(
        tile.image,
        view.x + (tile.x - view.imageWidth / 2) * view.zoom,
        view.y + (tile.y - view.imageHeight / 2) * view.zoom,
        tile.width * view.zoom,
        tile.height * view.zoom,
      )
    ctx.putImageData(this.output, 0, 0)
  }
  dispose() {
    this.image = null
    this.tiles = []
    this.output = null
  }
}

export function createRenderer(canvas, forceCanvas = false) {
  if (!forceCanvas) {
    try {
      const gl = canvas.getContext('webgl2', {
        alpha: false,
        antialias: false,
        depth: false,
        stencil: false,
        preserveDrawingBuffer: false,
        powerPreference: 'low-power',
      })
      if (gl) return new WebGLRenderer(canvas, gl)
    } catch {
      /* Unsupported/failed GPU initialization falls back to a fresh canvas. */
    }
  }
  // A canvas cannot switch context types after WebGL allocation.
  const fresh = canvas.cloneNode(false)
  canvas.replaceWith(fresh)
  return new CanvasRenderer(fresh)
}
