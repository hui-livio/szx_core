import test from 'node:test'
import assert from 'node:assert/strict'
import { Viewport } from '../web/viewport.js'
import { pixelRoi } from '../web/roi.js'

test('ROI uses original pixels after fit, pan, zoom and resize, independent of preview size', () => {
  const view = new Viewport()
  view.resize(900, 600)
  view.setImage(17952, 13200)
  const start = { x: 1234, y: 2345 }
  const end = { x: 4567, y: 6789 }
  const expected = { x: 1234, y: 2345, w: 3333, h: 4444 }
  const check = () => {
    const a = view.imagePoint(view.screenPoint(start))
    const b = view.imagePoint(view.screenPoint(end))
    assert.deepEqual(pixelRoi(a, b, view.imageWidth, view.imageHeight), expected)
    assert.deepEqual(pixelRoi(b, a, view.imageWidth, view.imageHeight), expected)
  }
  check()
  view.pan(-145, 93)
  view.zoomAt(150, 200, view.zoom * 3)
  check()
  view.resize(1200, 800)
  check()
  view.actualSize()
  check()
})

test('ROI clips every edge, snaps to integer pixels and rejects empty selections', () => {
  for (const [start, end, want] of [
    [
      { x: -30, y: -10 },
      { x: 999, y: 999 },
      { x: 0, y: 0, w: 320, h: 240 },
    ],
    [
      { x: 319.8, y: 239.9 },
      { x: 10.2, y: 20.4 },
      { x: 10, y: 20, w: 310, h: 220 },
    ],
    [{ x: 30, y: 40 }, { x: 30, y: 40 }, null],
    [{ x: 30, y: 40 }, { x: 30, y: 90 }, null],
    [{ x: 10.1, y: 20.1 }, { x: 10.2, y: 20.2 }, null],
    [{ x: 350, y: 10 }, { x: 400, y: 20 }, null],
  ]) {
    assert.deepEqual(pixelRoi(start, end, 320, 240), want)
  }
})
