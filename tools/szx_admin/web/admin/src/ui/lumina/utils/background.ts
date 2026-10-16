/** Background-only budgets; UI/chart frame rates are not affected. */
export const LUMINA_BACKGROUND_FPS = 30
export const LUMINA_BACKGROUND_SPARKS = 8
export const LUMINA_BACKGROUND_ORB_PIXELS = 768

export function backgroundParticleCount(width: number): number {
  return width <= 720 ? 5 : LUMINA_BACKGROUND_SPARKS
}

/** Bound each sprite's longest edge in physical pixels, including high-DPI screens. */
export function backgroundSpriteLimit(preferred: number, pixels: number, ratio: number): number {
  const dpr = Number.isFinite(ratio) && ratio > 0 ? ratio : 1
  return Math.min(preferred, pixels / dpr)
}

/** Deterministic positions: screenshots and remounts use the same particle field. */
export function backgroundParticle(index: number, seconds: number, height: number) {
  const duration = 17 + (index % 7) * 2.4
  const progress = (seconds / duration + ((index * 2.63) % 19) / duration) % 1
  return {
    y: (1 - progress) * (height + 40) - 20,
    opacity: Math.min(progress / 0.1, (1 - progress) / 0.1, 1) * 0.75,
  }
}
