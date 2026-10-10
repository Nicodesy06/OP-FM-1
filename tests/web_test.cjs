// SPDX-License-Identifier: GPL-3.0-only
// End-to-end test of the browser simulator in Chromium (Playwright): power on, the real wasm in the
// AudioWorklet, notes from the computer keyboard, knobs, the four modes, tape recording and undo / redo.
//   platform/web/build.sh && (cd build/web && python3 -m http.server 8790 &)
//   NODE_PATH=$(npm root -g) node tests/web_test.cjs [http://localhost:8790/]
const { chromium } = require('playwright');

const URL = process.argv[2] || 'http://localhost:8790/';
let pass = 0, fail = 0;
const check = (c, msg) => { if (c) pass++; else { fail++; console.log('FAIL', msg); } };
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

(async () => {
  const browser = await chromium.launch({ args: ['--autoplay-policy=no-user-gesture-required'] });
  const page = await browser.newPage({ viewport: { width: 1100, height: 900 } });
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  page.on('console', (m) => { if (m.type() === 'error') errors.push(m.text()); });
  await page.goto(URL);
  await page.click('#powerbtn');
  await page.waitForFunction(() => window.tapeos && window.tapeos.ready && window.tapeos.frames > 3, null, { timeout: 15000 });
  const last = () => page.evaluate(() => window.tapeos.last);
  check((await last()).screen === 0, 'boots on SYNTH');
  await page.screenshot({ path: 'build/test/web_synth_idle.png' });

  // a note from the computer's keyboard: the DSP in the worklet makes sound (the master meter)
  await page.keyboard.down('k');
  await sleep(400);
  let m = await last();
  check(m.peak > 500, `a key sounds in the worklet (master peak ${m.peak})`);
  check(m.keys !== 0, 'the key lights');
  await page.screenshot({ path: 'build/test/web_synth_note.png' });
  await page.keyboard.up('k');
  await sleep(1500);
  m = await last();
  check(m.peak < 200, `and stops (peak ${m.peak})`);

  // a knob: wheel on KNOB1 (CUTOFF) changes the screen
  const before = await page.evaluate(() => document.getElementById('screen').toDataURL());
  const k1 = await page.$('[data-enc="2"]');
  const box = await k1.boundingBox();
  await page.mouse.move(box.x + box.width / 2, box.y + 20);
  for (let i = 0; i < 20; i++) await page.mouse.wheel(0, 100);
  await sleep(300);
  const after = await page.evaluate(() => document.getElementById('screen').toDataURL());
  check(before !== after, 'KNOB1 changes the screen (CUTOFF)');

  // the four modes
  for (const [key, scr, name] of [['2', 4, 'drum'], ['3', 5, 'tape'], ['4', 6, 'mixer'], ['1', 0, 'synth']]) {
    await page.keyboard.press(key);
    await sleep(250);
    check((await last()).screen === scr, `key ${key} -> screen ${scr}`);
    await page.screenshot({ path: `build/test/web_${name}.png` });
  }

  // TAPE: record (from stop), play two notes, stop recording, undo, redo
  await page.keyboard.press('3');
  await sleep(200);
  await page.keyboard.press('Enter');                       // REC
  await sleep(300);
  check(((await last()).playing & 3) === 3, 'REC arms and plays');
  for (const k of ['a', 'd', 'g']) { await page.keyboard.down(k); await sleep(150); await page.keyboard.up(k); await sleep(100); }
  await page.keyboard.press('Enter');                       // end the take
  await sleep(300);
  m = await last();
  check(m.events[0] === 3, `3 notes on track 1 (got ${m.events[0]})`);
  check(m.undo === 1, 'one undo level');
  await page.screenshot({ path: 'build/test/web_tape_recorded.png' });
  await page.keyboard.press('Control+z');
  await sleep(300);
  m = await last();
  check(m.events[0] === 0 && m.redo === 1, `Ctrl+Z undoes (events ${m.events[0]}, redo ${m.redo})`);
  await page.screenshot({ path: 'build/test/web_tape_undo.png' });
  await page.keyboard.press('Control+y');
  await sleep(300);
  m = await last();
  check(m.events[0] === 3, `Ctrl+Y redoes (events ${m.events[0]})`);
  await page.keyboard.press(' ');                           // stop
  await sleep(200);
  check(((await last()).playing & 1) === 0, 'SPACE stops');

  // the panel's own buttons (pointer): SEQ held + OCT- on screen = undo
  const seq = await page.$('[data-btn="11"]'), octd = await page.$('[data-btn="0"]');
  const sb = await seq.boundingBox(), ob = await octd.boundingBox();
  await page.mouse.move(sb.x + 10, sb.y + 10); await page.mouse.down();
  await sleep(60);
  await octd.dispatchEvent('pointerdown', { pointerId: 9 }); await sleep(60); await octd.dispatchEvent('pointerup', { pointerId: 9 });
  await sleep(60);
  await page.mouse.up();
  await sleep(300);
  m = await last();
  check(m.events[0] === 0, `on-screen SEQ + OCT- undoes too (events ${m.events[0]})`);
  void ob;

  check(errors.length === 0, 'no page errors: ' + errors.join(' | '));
  await browser.close();
  console.log(`web_test: ${pass} passed, ${fail} failed`);
  process.exit(fail ? 1 : 0);
})().catch((e) => { console.error(e); process.exit(2); });
