/** Center an already-positioned overlay, falling back to viewport-safe anchoring. */
export function alignLuminaPopupArrow(popup: HTMLElement, anchor: HTMLElement) {
  popup.style.translate = ''
  const popupBounds = popup.getBoundingClientRect()
  const anchorBounds = anchor.getBoundingClientRect()
  const center = anchorBounds.left + anchorBounds.width / 2
  const viewport = document.documentElement.clientWidth
  // Prefer a centered panel and centered arrow. Only move off-center when the
  // panel would otherwise overflow the viewport, retaining an 8px safety gutter.
  const left = Math.max(
    8,
    Math.min(viewport - popupBounds.width - 8, center - popupBounds.width / 2),
  )
  popup.style.translate = `${left - popupBounds.left}px 0`
  const inset = Math.min(20, popupBounds.width / 2)
  const x = Math.max(inset, Math.min(popupBounds.width - inset, center - left))
  const above = popupBounds.top < anchorBounds.top
  popup.style.setProperty('--lumina-popup-anchor-x', `${x}px`)
  popup.style.marginTop = above ? 'calc(-1 * var(--lg-s2))' : 'var(--lg-s2)'
  popup.dataset.side = above ? 'top' : 'bottom'
  popup.classList.add('lumina-popup-arrow')
}
