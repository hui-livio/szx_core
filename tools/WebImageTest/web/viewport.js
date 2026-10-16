export const MAX_IMAGE_PIXELS = 256 * 1024 * 1024

export function backingSize(width, height, devicePixelRatio = 1) {
  const w = Math.max(1, width)
  const h = Math.max(1, height)
  const dpr = Number.isFinite(devicePixelRatio) && devicePixelRatio > 0 ? devicePixelRatio : 1
  const ratio = Math.min(dpr, 2, 8192 / w, 8192 / h, Math.sqrt((16 * 1024 * 1024) / (w * h)))
  return { width: Math.max(1, Math.floor(w * ratio)), height: Math.max(1, Math.floor(h * ratio)) }
}

/** Coordinates are CSS pixels. zoom=1 means one image pixel per CSS pixel. */
export class Viewport {
  width = 1
  height = 1
  imageWidth = 0
  imageHeight = 0
  zoom = 1
  x = 0
  y = 0

  resize(width, height) {
    if (!(width > 0 && height > 0)) return
    this.x += (width - this.width) / 2
    this.y += (height - this.height) / 2
    this.width = width
    this.height = height
  }

  setImage(width, height) {
    if (
      !Number.isSafeInteger(width) ||
      !Number.isSafeInteger(height) ||
      width < 1 ||
      height < 1 ||
      width * height > MAX_IMAGE_PIXELS
    ) {
      throw new Error('图片尺寸超出支持范围。')
    }
    this.imageWidth = width
    this.imageHeight = height
    this.fit()
  }

  fit() {
    if (!this.imageWidth) return
    this.zoom = Math.min(
      64,
      Math.max(
        1 / 4096,
        Math.min(this.width / this.imageWidth, this.height / this.imageHeight) * 0.92,
      ),
    )
    this.center()
  }

  center() {
    this.x = this.width / 2
    this.y = this.height / 2
  }

  actualSize() {
    this.zoom = 1
    this.center()
  }

  zoomAt(x, y, zoom) {
    if (!this.imageWidth || !Number.isFinite(zoom)) return
    const next = Math.min(64, Math.max(1 / 4096, zoom))
    const ratio = next / this.zoom
    this.x = x - (x - this.x) * ratio
    this.y = y - (y - this.y) * ratio
    this.zoom = next
  }

  pan(dx, dy) {
    if (Number.isFinite(dx) && Number.isFinite(dy)) {
      this.x += dx
      this.y += dy
    }
  }
}
