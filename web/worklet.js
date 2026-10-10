// SPDX-License-Identifier: GPL-3.0-only
// FM1 Tape OS in an AudioWorklet: tapeos.wasm is the shared C application (src/tapeos.c, platform/web/web.c).
// Each render quantum asks it for 128 frames, which also runs the device's main loop for every millisecond
// those frames cover. Between quanta the page's input goes in; about 30 times a second the screen, the
// lights and the status go out. The design follows Felucca's browser emulator (web/emu/worklet.js).

const HOLD_MS = 20;              // a press reaches the device for at least this long (a quick tap still plays)

class TapeOS extends AudioWorkletProcessor {
  constructor() {
    super();
    this.ex = null;
    this.queue = [];               // panel events, applied in order
    this.downAt = new Map();       // "kind:id" -> device ms of its press
    this.lastSerial = -1;
    this.lastPost = 0;
    this.port.onmessage = (e) => this.onMessage(e.data);
  }

  async onMessage(m) {
    if (m.type === 'load') {
      const { instance } = await WebAssembly.instantiate(m.wasm, {});
      this.ex = instance.exports;
      this.ex.web_init();
      this.port.postMessage({ type: 'ready' });
    } else if (m.type === 'panel') {
      this.queue.push(m);
    } else if (m.type === 'midi') {
      if (this.ex) this.ex.web_midi(m.st, m.d1, m.d2);
    }
  }

  applyQueue() {
    const ex = this.ex, now = ex.web_ms();
    while (this.queue.length) {
      const m = this.queue[0];
      const isUp = m.kind === 1 || m.kind === 3, key = (isUp ? m.kind - 1 : m.kind) + ':' + m.id;
      if (isUp && this.downAt.has(key) && now - this.downAt.get(key) < HOLD_MS) break;   // too soon: next quantum
      this.queue.shift();
      if (!isUp && m.kind !== 4) this.downAt.set(key, now);
      ex.web_panel(m.kind, m.id, m.v | 0);
    }
  }

  process(inputs, outputs) {
    const ex = this.ex, out = outputs[0];
    if (!ex) return true;
    const n = out[0].length;
    this.applyQueue();
    ex.web_render(n);
    const mem = ex.memory.buffer;
    out[0].set(new Float32Array(mem, ex.web_out_l(), n));
    if (out[1]) out[1].set(new Float32Array(mem, ex.web_out_r(), n));
    const ms = ex.web_ms(), serial = ex.web_fb_serial();
    if (serial !== this.lastSerial && ms - this.lastPost >= 30) {
      this.lastSerial = serial;
      this.lastPost = ms;
      const fb = new Uint16Array(mem, ex.web_fb(), 240 * 240).slice();
      const und = ex.web_undo_levels();
      this.port.postMessage({
        type: 'frame', fb, ms,
        keys: ex.web_leds_keys(), buttons: ex.web_leds_buttons(),
        screen: ex.web_screen(), playing: ex.web_playing(), pos: ex.web_tape_pos(),
        undo: und & 255, redo: und >> 8,
        events: [0, 1, 2, 3].map((k) => ex.web_events(k)), peak: ex.web_peak(),
      }, [fb.buffer]);
    }
    return true;
  }
}

registerProcessor('tapeos', TapeOS);
