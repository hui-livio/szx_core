/** Integer original-image bounds, snapped to pixel edges and clipped to the image. */
export function pixelRoi(start, end, width, height) {
  const clamp = (value, limit) => Math.max(0, Math.min(limit, Math.round(value)))
  const left = clamp(Math.min(start.x, end.x), width)
  const top = clamp(Math.min(start.y, end.y), height)
  const right = clamp(Math.max(start.x, end.x), width)
  const bottom = clamp(Math.max(start.y, end.y), height)
  if (right <= left || bottom <= top) return null
  return { x: left, y: top, w: right - left, h: bottom - top }
}
