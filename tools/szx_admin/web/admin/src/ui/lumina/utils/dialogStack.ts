interface DialogEntry {
  element: () => HTMLElement | null
  requestClose: () => void
}

const dialogs: DialogEntry[] = []

export function topLuminaDialog(): HTMLElement | null {
  return dialogs.at(-1)?.element() ?? null
}

function onEscape(event: KeyboardEvent) {
  if ((event.key !== 'Escape' && event.code !== 'Escape') || event.isComposing || event.repeat)
    return
  const entry = dialogs.at(-1)
  const dialog = entry?.element()
  if (!entry || !dialog) return

  // Select, DatePicker, and Menu overlays are teleported outside the dialog.
  // Let an open child popup consume Escape before closing its parent.
  const children = [
    ...dialog.querySelectorAll<HTMLElement>('[aria-expanded="true"][aria-controls]'),
  ]
    .map((owner) => {
      const id = owner.getAttribute('aria-controls')
      return { owner, popup: id ? document.getElementById(id) : null }
    })
    .filter((item): item is { owner: HTMLElement; popup: HTMLElement } => item.popup !== null)
  const target = event.target instanceof Node ? event.target : null
  const child = children.find(
    ({ owner, popup }) => target && (owner.contains(target) || popup.contains(target)),
  )

  if (child) return

  if (children.length > 0) {
    event.preventDefault()
    event.stopPropagation()
    const popup = children.at(-1)!.popup
    const receiver = popup.querySelector<HTMLElement>('[role="menu"]') ?? popup
    receiver.dispatchEvent(
      new KeyboardEvent('keydown', {
        key: 'Escape',
        code: 'Escape',
        bubbles: true,
        cancelable: true,
      }),
    )
    return
  }

  // Capture also handles closed Select/DatePicker inputs that stop propagation.
  // Only the top registration handles the key, even if its owner vetoes close.
  event.preventDefault()
  event.stopPropagation()
  entry.requestClose()
}

export function registerLuminaDialog(entry: DialogEntry): () => void {
  dialogs.push(entry)
  if (dialogs.length === 1) document.addEventListener('keydown', onEscape, true)
  return () => {
    const index = dialogs.indexOf(entry)
    if (index === -1) return
    dialogs.splice(index, 1)
    if (dialogs.length === 0) document.removeEventListener('keydown', onEscape, true)
  }
}
