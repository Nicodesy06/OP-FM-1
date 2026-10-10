/* SPDX-License-Identifier: GPL-3.0-only */
/* The panel: the FM1's controls -> the instrument's actions. The same mapping for the FM1 and the browser
 * (the simulator sends the same physical controls). Runs on the main loop; it only reads the audio side and
 * talks to it through cmd_push().
 *
 *   HOME   SYNTH          ENV / LFO / FX   the synth's pages        SEL   DRUM
 *   SEQ    TAPE           GLO   MIXER      EDIT (held)   the knobs' second row
 *   PLAY   play / stop    REC   record on the selected track (from stop: arm and play)
 *   SEQ held + OCT- / OCT+   UNDO / REDO   (as #248's TAPE + left / right)
 *   SEQ held + PLAY          back to the start
 *   SAVE held + REC          erase the selected track (undoable)
 *   GLO held + key 1..4 (F3 G3 A3 B3)   mute channel 1..4
 *   OCT- / OCT+              the keyboard's octave
 *   KNOB 1..4                the four coloured parameters
 *   SELECT                   SYNTH pages: the next page; TAPE: the track; MIXER: master
 *   ALGORITHM                SYNTH: WAVE; DRUM: the sound to edit
 *   PRESETS                  SYNTH: preset; TAPE: move the playhead by beats (stopped) */

#define KEY0 53                  /* key 0 = F3 */
static uint32_t btn_held;
static struct { uint8_t kind, val; } key_sent[TOS_NKEYS];   /* kind 0 none, 1 note, 2 drum */
static uint32_t fb_ser, last_draw;
static uint8_t inited;

static const uint8_t WHITE_KEY[TOS_NKEYS] = {     /* F3..G5: the index of each white key, 255 = black */
    0, 255, 1, 255, 2, 255, 3, 4, 255, 5, 255, 6, 7, 255, 8, 255, 9, 255, 10, 11, 255, 12, 255, 13, 14, 255, 15};

static void set_screen(uint32_t s)
{
    ui.scr = (uint8_t)s;
    if (s == SCR_DRUM) ui.instr_drum = 1;
    else if (s <= SCR_LFO) ui.instr_drum = 0;
}

static uint32_t held(uint32_t b) { return btn_held >> b & 1u; }

static void key_down(uint32_t k)
{
    if (held(TOS_BTN_GLO) && WHITE_KEY[k] < 4u) {          /* mute */
        cmd_push(CMD_PARAM, PR_MUTE1 + WHITE_KEY[k], mx_mute[WHITE_KEY[k]] ? -1 : 1);
        return;
    }
    if (ui.instr_drum) {
        uint32_t d = WHITE_KEY[k] != 255u ? WHITE_KEY[k] % NDRUM : WHITE_KEY[k - 1u] % NDRUM;
        key_sent[k].kind = 2;
        key_sent[k].val = (uint8_t)d;
        cmd_push(CMD_DRUM_ON, d, WHITE_KEY[k] != 255u ? 100 : 127);   /* black keys: the same hit, accented */
    } else {
        int32_t n = KEY0 + 12 * ui.oct + (int32_t)k;
        n = clamp(n, 0, 127);
        key_sent[k].kind = 1;
        key_sent[k].val = (uint8_t)n;
        cmd_push(CMD_NOTE_ON, (uint32_t)n, 100);
    }
}

static void key_up(uint32_t k)
{
    if (key_sent[k].kind == 1) cmd_push(CMD_NOTE_OFF, key_sent[k].val, 0);
    else if (key_sent[k].kind == 2) cmd_push(CMD_DRUM_OFF, key_sent[k].val, 0);
    key_sent[k].kind = 0;
}

static void btn_down(uint32_t b)
{
    switch (b) {
    case TOS_BTN_HOME: set_screen(SCR_SYNTH); break;
    case TOS_BTN_ENV: set_screen(SCR_ENV); break;
    case TOS_BTN_LFO: set_screen(SCR_LFO); break;
    case TOS_BTN_FX: set_screen(SCR_FX); break;
    case TOS_BTN_SEL: set_screen(SCR_DRUM); break;
    case TOS_BTN_SEQ: set_screen(SCR_TAPE); break;
    case TOS_BTN_GLO: set_screen(SCR_MIXER); break;
    case TOS_BTN_EDIT: ui.shift = 1; break;
    case TOS_BTN_PLAY: cmd_push(held(TOS_BTN_SEQ) ? CMD_REWIND : CMD_PLAY, 0, 0); break;
    case TOS_BTN_REC: cmd_push(held(TOS_BTN_SAVE) ? CMD_ERASE_TRACK : CMD_REC, 0, 0); break;
    case TOS_BTN_OCT_DOWN:
        if (held(TOS_BTN_SEQ)) cmd_push(CMD_UNDO, 0, 0);
        else if (ui.oct > -2) ui.oct--;
        break;
    case TOS_BTN_OCT_UP:
        if (held(TOS_BTN_SEQ)) cmd_push(CMD_REDO, 0, 0);
        else if (ui.oct < 2) ui.oct++;
        break;
    default: break;
    }
}

static void enc(uint32_t e, int32_t d)
{
    if (!d) return;
    if (e >= TOS_ENC_KNOB1 && e <= TOS_ENC_KNOB4) {
        cmd_push(CMD_PARAM, KNOBS[ui.scr][ui.shift][e - TOS_ENC_KNOB1], d);
        return;
    }
    if (e == TOS_ENC_SELECT) {
        if (ui.scr <= SCR_FX) set_screen((uint32_t)(((int32_t)ui.scr + (d > 0 ? 1 : 3)) % 4));
        else if (ui.scr == SCR_TAPE) cmd_push(CMD_PARAM, PR_TRACK, d > 0 ? 1 : -1);
        else if (ui.scr == SCR_MIXER) cmd_push(CMD_PARAM, PR_MASTER, d);
        else if (ui.scr == SCR_DRUM) cmd_push(CMD_PARAM, PR_DRSEL, d > 0 ? 1 : -1);
    } else if (e == TOS_ENC_ALGORITHM) {
        if (ui.scr == SCR_DRUM) cmd_push(CMD_PARAM, PR_DRSEL, d > 0 ? 1 : -1);
        else cmd_push(CMD_PARAM, PR_WAVE, d > 0 ? 1 : -1);
    } else if (e == TOS_ENC_PRESETS) {
        if (ui.scr == SCR_TAPE) cmd_push(CMD_SEEK, 0, d);
        else if (ui.scr == SCR_MIXER) cmd_push(CMD_PARAM, PR_MASTER, d);
        else if (ui.scr == SCR_DRUM) cmd_push(CMD_PARAM, PR_DRSEL, d > 0 ? 1 : -1);
        else cmd_push(CMD_PARAM, PR_PRESET, d > 0 ? 1 : -1);
    }
}

void tos_panel(uint32_t kind, uint32_t id, int32_t value)
{
    switch (kind) {
    case TOS_PE_BTN_DOWN:
        if (id < TOS_NBTN) { btn_held |= 1u << id; btn_down(id); }
        break;
    case TOS_PE_BTN_UP:
        if (id < TOS_NBTN) {
            btn_held &= ~(1u << id);
            if (id == TOS_BTN_EDIT) ui.shift = 0;
        }
        break;
    case TOS_PE_KEY_DOWN: if (id < TOS_NKEYS) key_down(id); break;
    case TOS_PE_KEY_UP: if (id < TOS_NKEYS) key_up(id); break;
    case TOS_PE_ENC: if (id < TOS_NENC) enc(id, value); break;
    default: break;
    }
}

/* MIDI in: notes play the instrument on screen (synth or drums), as the keys do, at their velocity */
static uint8_t midi_drum[128];
void tos_midi(uint32_t status, uint32_t d1, uint32_t d2)
{
    uint32_t t = status & 0xF0u;
    d1 &= 127u;
    if (t == 0x90u && d2) {
        midi_drum[d1] = ui.instr_drum;
        if (ui.instr_drum) cmd_push(CMD_DRUM_ON, d1 % NDRUM, (int32_t)d2);
        else cmd_push(CMD_NOTE_ON, d1, (int32_t)d2);
    } else if (t == 0x80u || t == 0x90u) {
        if (midi_drum[d1]) cmd_push(CMD_DRUM_OFF, d1 % NDRUM, 0);
        else cmd_push(CMD_NOTE_OFF, d1, 0);
    }
}

void tos_init(void)
{
    uint32_t k;
    engine_init();
    for (k = 0; k < TOS_NKEYS; k++) key_sent[k].kind = 0;
    btn_held = 0;
    ui.scr = SCR_SYNTH;
    ui.instr_drum = 0;
    ui.shift = 0;
    ui.oct = 0;
    ui.popup = 0;
    ui.frame = 0;
    last_draw = 0;
    render_frame();
    fb_ser++;
    inited = 1;
}

#ifndef TOS_FRAME_MS
#define TOS_FRAME_MS 33u         /* ~30 frames a second */
#endif

void tos_ui(uint32_t ms)
{
    uint32_t p = ev_popup;
    ui.ms = ms;
    if (p) { ui.popup = (uint8_t)p; ui.popup_until = ms + 900u; ev_popup = 0; }
    if (ui.popup && (int32_t)(ms - ui.popup_until) >= 0) ui.popup = 0;
    if (ms - last_draw >= TOS_FRAME_MS) {
        last_draw = ms;
        render_frame();
        fb_ser++;
    }
}

void tos_cpu(uint32_t permille) { cpu_permille = permille; }
uint16_t *tos_fb(void) { return fb; }
uint32_t tos_fb_serial(void) { return fb_ser; }

uint32_t tos_leds_keys(void)
{
    uint32_t i, m = 0;
    for (i = 0; i < NSVOICE; i++)
        if (sv_view[i].active && sv[i].stage != 3 && sv_view[i].src == SRC_LIVE) {
            int32_t k = sv_view[i].note - KEY0 - 12 * ui.oct;
            if (k >= 0 && k < (int32_t)TOS_NKEYS) m |= 1u << k;
        }
    for (i = 0; i < TOS_NKEYS; i++)
        if (key_sent[i].kind == 2) m |= 1u << i;
    return m;
}

uint32_t tos_leds_buttons(void)
{
    static const uint8_t SCR_BTN[NSCR] = {TOS_BTN_HOME, TOS_BTN_ENV, TOS_BTN_LFO, TOS_BTN_FX, TOS_BTN_SEL,
                                         TOS_BTN_SEQ, TOS_BTN_GLO};
    uint32_t m = 1u << SCR_BTN[ui.scr];
    if (tp.playing) m |= 1u << TOS_BTN_PLAY;
    if (tp.armed) m |= 1u << TOS_BTN_REC;
    if (ui.oct < 0) m |= 1u << TOS_BTN_OCT_DOWN;
    if (ui.oct > 0) m |= 1u << TOS_BTN_OCT_UP;
    return m;
}
