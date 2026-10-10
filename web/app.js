// SPDX-License-Identifier: GPL-3.0-only
// The simulator's page: the FM1's panel (buttons, encoders, keys) -> the worklet as the FM1's physical
// controls (the C side maps them, as on the device); the worklet's frames -> the 240 x 240 canvas.

const PE = { BTN_DOWN: 0, BTN_UP: 1, KEY_DOWN: 2, KEY_UP: 3, ENC: 4 };
const BTN = { OCT_DOWN: 0, OCT_UP: 1, FX: 2, SEL: 3, ENV: 4, LFO: 5, EDIT: 6, GLO: 7, HOME: 8, SAVE: 9,
              ARP: 10, SEQ: 11, PLAY: 12, REC: 13 };
let node = null, ctx = null, master = null;

const send = (m) => { if (node) node.port.postMessage(m); };
const panel = (kind, id, v = 0) => send({ type: 'panel', kind, id, v });

// ---------------------------------------------------------------- the screen
const canvas = document.getElementById('screen');
const g = canvas.getContext('2d');
const img = g.createImageData(240, 240);
function drawScreen(fb) {
  const d = img.data;
  for (let i = 0; i < 240 * 240; i++) {
    const v = fb[i];
    d[4 * i] = ((v >> 11) & 31) * 255 / 31;
    d[4 * i + 1] = ((v >> 5) & 63) * 255 / 63;
    d[4 * i + 2] = (v & 31) * 255 / 31;
    d[4 * i + 3] = 255;
  }
  g.putImageData(img, 0, 0);
}

// ---------------------------------------------------------------- the keyboard (F3 .. G5: 27 keys)
const kb = document.getElementById('keyboard');
const keyEls = [];
const BLACK = [1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25];        // F#3 G#3 A#3 C#4 D#4 F#4 G#4 A#4 C#5 D#5 F#5
let whiteIdx = 0;
for (let k = 0; k < 27; k++) {
  const el = document.createElement('div');
  const black = BLACK.includes(k);
  el.className = black ? 'key black' : 'key white';
  if (black) el.style.left = `calc(${whiteIdx} * (100% / 16) - (100% / 16) * 0.32)`;
  else el.style.left = `calc(${whiteIdx++} * (100% / 16))`;
  el.dataset.key = k;
  kb.appendChild(el);
  keyEls.push(el);
}
const keyDown = new Set();
function keyPress(k, on) {
  if (on === keyDown.has(k)) return;
  if (on) keyDown.add(k); else keyDown.delete(k);
  keyEls[k].classList.toggle('pressed', on);
  panel(on ? PE.KEY_DOWN : PE.KEY_UP, k);
}
function pointerKeys(el, down, up) {
  el.addEventListener('pointerdown', (e) => { try { el.setPointerCapture(e.pointerId); } catch (_) { /* synthetic */ } down(e); e.preventDefault(); });
  el.addEventListener('pointerup', up);
  el.addEventListener('pointercancel', up);
}
keyEls.forEach((el, k) => pointerKeys(el, () => keyPress(k, true), () => keyPress(k, false)));

// ---------------------------------------------------------------- buttons
const btnEls = {};
document.querySelectorAll('[data-btn]').forEach((el) => {
  const id = +el.dataset.btn;
  btnEls[id] = el;
  pointerKeys(el, () => { el.classList.add('pressed'); panel(PE.BTN_DOWN, id); },
                  () => { el.classList.remove('pressed'); panel(PE.BTN_UP, id); });
});

// ---------------------------------------------------------------- encoders (wheel, drag)
document.querySelectorAll('.enc').forEach((el) => {
  const id = el.dataset.enc;
  const cap = document.createElement('div');
  cap.className = 'cap';
  el.appendChild(cap);
  const lab = document.createElement('span');
  lab.textContent = el.dataset.label;
  el.appendChild(lab);
  let angle = 0, acc = 0, lastY = 0;
  const turn = (d) => {
    if (!d) return;
    angle += d * 12;
    cap.style.transform = `rotate(${angle}deg)`;
    if (id === 'master') { setMaster(d); return; }
    panel(PE.ENC, +id, d);
  };
  el.addEventListener('wheel', (e) => { e.preventDefault(); turn(e.deltaY < 0 ? 1 : -1); }, { passive: false });
  el.addEventListener('pointerdown', (e) => { try { el.setPointerCapture(e.pointerId); } catch (_) { /* synthetic */ } lastY = e.clientY; acc = 0; e.preventDefault(); });
  el.addEventListener('pointermove', (e) => {
    if (!el.hasPointerCapture(e.pointerId)) return;
    acc += lastY - e.clientY;
    lastY = e.clientY;
    while (acc >= 6) { turn(1); acc -= 6; }
    while (acc <= -6) { turn(-1); acc += 6; }
  });
});
let masterLevel = 0.8;
function setMaster(d) {                                    // the MASTER knob: the output level (as on the FM1)
  masterLevel = Math.min(1, Math.max(0, masterLevel + d * 0.04));
  if (master) master.gain.value = masterLevel;
}

// ---------------------------------------------------------------- the computer's keyboard
const PIANO = 'awsedftgyhujkolp;';                          // F3 F#3 G3 G#3 A3 A#3 B3 C4 ... (17 keys)
const PIANO_MAP = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16];
const SHORT = { ' ': BTN.PLAY, Enter: BTN.REC, z: BTN.OCT_DOWN, x: BTN.OCT_UP, '1': BTN.HOME, '2': BTN.SEL,
                '3': BTN.SEQ, '4': BTN.GLO, '5': BTN.ENV, '6': BTN.LFO, '7': BTN.FX };
const tapBtn = (id) => { panel(PE.BTN_DOWN, id); panel(PE.BTN_UP, id); };
function combo(hold, id) { panel(PE.BTN_DOWN, hold); panel(PE.BTN_DOWN, id); panel(PE.BTN_UP, id); panel(PE.BTN_UP, hold); }
window.addEventListener('keydown', (e) => {
  if (e.repeat || !node) return;
  const k = e.key.length === 1 ? e.key.toLowerCase() : e.key;
  if ((e.ctrlKey || e.metaKey) && (k === 'z' || k === 'y')) {
    e.preventDefault();
    combo(BTN.SEQ, k === 'y' || e.shiftKey ? BTN.OCT_UP : BTN.OCT_DOWN);
    return;
  }
  if (e.ctrlKey || e.metaKey || e.altKey) return;
  if (e.key === 'Shift') { panel(PE.BTN_DOWN, BTN.EDIT); return; }
  const p = PIANO.indexOf(k);
  if (p >= 0) { keyPress(PIANO_MAP[p], true); e.preventDefault(); return; }
  if (k in SHORT) { e.preventDefault(); panel(PE.BTN_DOWN, SHORT[k]); }
});
window.addEventListener('keyup', (e) => {
  if (!node) return;
  const k = e.key.length === 1 ? e.key.toLowerCase() : e.key;
  if (e.key === 'Shift') { panel(PE.BTN_UP, BTN.EDIT); return; }
  const p = PIANO.indexOf(k);
  if (p >= 0) { keyPress(PIANO_MAP[p], false); return; }
  if (k in SHORT) panel(PE.BTN_UP, SHORT[k]);
});

// ---------------------------------------------------------------- frames from the device
const status = document.getElementById('status');
const SCREENS = ['SYNTH', 'ENVELOPE', 'LFO', 'FX', 'DRUM', 'TAPE', 'MIXER'];
window.tapeos = { last: null, frames: 0 };                  // read by the browser tests
function onFrame(m) {
  drawScreen(m.fb);
  window.tapeos.last = m;
  window.tapeos.frames++;
  for (let k = 0; k < 27; k++) keyEls[k].classList.toggle('lit', !!(m.keys >> k & 1));
  for (const [id, el] of Object.entries(btnEls)) el.classList.toggle('lit', !!(m.buttons >> id & 1));
  status.textContent = `${SCREENS[m.screen]} · ${m.playing & 1 ? 'PLAY' : 'STOP'}${m.playing & 2 ? ' · REC' : ''}` +
    ` · nastro ${(m.pos * 32 / 44100).toFixed(2)} s · eventi ${m.events.join('/')} · undo ${m.undo} redo ${m.redo}`;
}

// ---------------------------------------------------------------- power on
async function powerOn() {
  const btn = document.getElementById('powerbtn');
  btn.disabled = true;
  ctx = new AudioContext({ sampleRate: 44100, latencyHint: 'interactive' });
  await ctx.audioWorklet.addModule('worklet.js');
  const wasm = await (await fetch('tapeos.wasm')).arrayBuffer();
  node = new AudioWorkletNode(ctx, 'tapeos', { numberOfInputs: 0, outputChannelCount: [2] });
  master = new GainNode(ctx, { gain: masterLevel });
  node.connect(master).connect(ctx.destination);
  node.port.onmessage = (e) => {
    if (e.data.type === 'frame') onFrame(e.data);
    if (e.data.type === 'ready') { document.getElementById('power').hidden = true; window.tapeos.ready = true; }
  };
  node.port.postMessage({ type: 'load', wasm }, [wasm]);
  await ctx.resume();
  if (navigator.requestMIDIAccess) {                       // Web MIDI: optional
    navigator.requestMIDIAccess().then((acc) => {
      for (const inp of acc.inputs.values())
        inp.onmidimessage = (ev) => { const [st, d1, d2] = ev.data; send({ type: 'midi', st, d1, d2: d2 ?? 0 }); };
    }).catch(() => {});
  }
}
document.getElementById('powerbtn').addEventListener('click', () => powerOn().catch((err) => {
  status.textContent = 'errore: ' + err.message;
}));
