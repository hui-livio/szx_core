import { liveFrame } from './images.js'

/** Pace requests by their start time. At most one fetch and one next-frame timer. */
export class LiveStream {
  constructor(onFrame, onError, request = liveFrame) {
    this.onFrame = onFrame
    this.onError = onError
    this.request = request
    this.fps = 10
    this.running = false
    this.generation = 0
    this.timer = null
    this.controller = null
    this.lastStarted = -Infinity
  }
  setFps(fps) {
    if (!Number.isInteger(fps) || fps < 1 || fps > 60)
      throw new Error('帧率必须是 1–60 之间的整数。')
    this.fps = fps
    if (this.running && !this.controller) this.schedule()
  }
  start() {
    this.stop()
    this.running = true
    this.lastStarted = -Infinity
    this.schedule()
  }
  stop() {
    this.running = false
    this.generation++
    clearTimeout(this.timer)
    this.timer = null
    this.controller?.abort()
    this.controller = null
  }
  schedule() {
    clearTimeout(this.timer)
    this.timer = setTimeout(
      () => void this.receive(),
      Math.max(0, this.lastStarted + 1000 / this.fps - performance.now()),
    )
  }
  async receive() {
    this.timer = null
    if (!this.running || this.controller) return
    const generation = this.generation
    const controller = new AbortController()
    this.controller = controller
    this.lastStarted = performance.now()
    try {
      const frame = await this.request(controller.signal)
      if (generation === this.generation) this.onFrame(frame)
    } catch (error) {
      if (generation === this.generation) {
        this.stop()
        this.onError(error)
      }
    } finally {
      if (generation === this.generation) {
        this.controller = null
        if (this.running) this.schedule()
      }
    }
  }
}
