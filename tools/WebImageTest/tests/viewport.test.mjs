import test from 'node:test'
import assert from 'node:assert/strict'
import { Viewport, backingSize, MAX_IMAGE_PIXELS } from '../web/viewport.js'

test('fit, 1:1 and resize keep image centred in CSS pixels', () => {
  const view = new Viewport()
  view.resize(1000, 600)
  view.setImage(2000, 1000)
  assert.equal(view.zoom, 0.46)
  assert.equal(view.x, 500)
  assert.equal(view.y, 300)
  view.actualSize()
  assert.equal(view.zoom, 1)
  view.resize(1200, 800)
  assert.equal(view.x, 600)
  assert.equal(view.y, 400)
})

test('zoom preserves the image point under the cursor; pan and zoom are finite and bounded', () => {
  const view = new Viewport()
  view.resize(1000, 600)
  view.setImage(1000, 600)
  const point = { x: 123, y: 456 }
  const before = (point.x - view.x) / view.zoom
  view.zoomAt(point.x, point.y, view.zoom * 3)
  assert.ok(Math.abs((point.x - view.x) / view.zoom - before) < 1e-8)
  const previous = view.x
  view.pan(25, -15)
  assert.equal(view.x, previous + 25)
  view.zoomAt(0, 0, Infinity)
  assert.ok(Number.isFinite(view.zoom))
  view.zoomAt(0, 0, 1e100)
  assert.equal(view.zoom, 64)
  view.zoomAt(0, 0, -1)
  assert.equal(view.zoom, 1 / 4096)
  const x = view.x
  view.pan(NaN, 0)
  assert.equal(view.x, x)
})

test('validates image dimensions and pixel budget before view mutation', () => {
  const view = new Viewport()
  for (const [w, h] of [
    [0, 10],
    [-1, 10],
    [NaN, 10],
    [10.5, 10],
    [MAX_IMAGE_PIXELS + 1, 1],
  ]) {
    assert.throws(() => view.setImage(w, h))
  }
  assert.equal(view.imageWidth, 0)
  view.setImage(8192, 8192)
  assert.equal(view.imageWidth, 8192)
})

test('backing store stays bounded on high DPI and huge displays', () => {
  for (const ratio of [1, 1.25, 2, 3, 4, NaN, 0]) {
    for (const [width, height] of [
      [320, 480],
      [1920, 1080],
      [3840, 2160],
      [16000, 12000],
    ]) {
      const size = backingSize(width, height, ratio)
      assert.ok(size.width <= 8192)
      assert.ok(size.height <= 8192)
      assert.ok(size.width * size.height <= 16 * 1024 * 1024)
      assert.ok(size.width >= 1 && size.height >= 1)
    }
  }
})
