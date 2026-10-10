/* SPDX-License-Identifier: GPL-3.0-only */
/* The screens. Every picture is drawn from the instrument's real state: the waveform from the engine's own
 * oscillators (dsp.c), the scope from the audio of the selected channel, the voices' envelopes, the drum
 * voices' levels, the meters, the tape's events and position. Nothing moves by itself. */

enum { SCR_SYNTH, SCR_ENV, SCR_LFO, SCR_FX, SCR_DRUM, SCR_TAPE, SCR_MIXER, NSCR };
static volatile uint32_t cpu_permille;
static const char *const SCR_NAME[NSCR] = {"SYNTH", "ENVELOPE", "LFO", "FX", "DRUM", "TAPE", "MIXER"};

/* the four knobs of each screen; EDIT held: the second row */
static const uint8_t KNOBS[NSCR][2][4] = {
    [SCR_SYNTH] = {{PR_CUT, PR_RES, PR_DTN, PR_DRV}, {PR_MIX, PR_NOIS, PR_KTR, PR_FTYPE}},
    [SCR_ENV] = {{PR_ATK, PR_DEC, PR_SUS, PR_REL}, {PR_FENV, PR_DEC, PR_SUS, PR_REL}},
    [SCR_LFO] = {{PR_LRATE, PR_LDEPTH, PR_LDEST, PR_LWAVE}, {PR_LRATE, PR_LDEPTH, PR_LDEST, PR_LWAVE}},
    [SCR_FX] = {{PR_FXTIME, PR_FXFDBK, PR_FXMIX, PR_FXTONE}, {PR_FXTIME, PR_FXFDBK, PR_FXMIX, PR_FXTONE}},
    [SCR_DRUM] = {{PR_DTUNE, PR_DDECAY, PR_DTONE, PR_DLEVEL}, {PR_DTUNE, PR_DDECAY, PR_DTONE, PR_DLEVEL}},
    [SCR_TAPE] = {{PR_TRACK, PR_BARS, PR_BPM, PR_LOOP}, {PR_TRACK, PR_BARS, PR_BPM, PR_LOOP}},
    [SCR_MIXER] = {{PR_VOL1, PR_VOL2, PR_VOL3, PR_VOL4}, {PR_PAN1, PR_PAN2, PR_PAN3, PR_PAN4}},
};

static struct {
    uint8_t scr, instr_drum, shift;
    int8_t oct;
    uint8_t popup;
    uint32_t popup_until, ms, frame;
} ui;

static const char *pval(uint32_t id, char *buf)
{
    int32_t v = param_get(id);
    if (PD[id].names) return PD[id].names[v - PD[id].min];
    if (id == PR_TRACK) v++;
    return itoa_s(v, buf);
}

/* ---- the common parts: title bar, transport, the four dials */
static void draw_header(void)
{
    char b[16];
    uint32_t beat, bar, tpb = ticks_per_beat_x10();
    int32_t x;
    text(6, 4, SCR_NAME[ui.scr], C_FG, 1);
    if (ui.scr <= SCR_FX) text(6 + (int32_t)txt_w(SCR_NAME[ui.scr], 1) + 8, 4, "ANALOG", C_DIM, 1);
    beat = tp.pos * 10u / tpb;
    bar = beat / 4u + 1u;
    itoa_s((int32_t)bar, b);
    x = 150;
    x = text(x, 4, "BAR ", C_DIM, 1);
    x = text(x, 4, b, C_FG, 1);
    x = text(x, 4, ".", C_DIM, 1);
    itoa_s((int32_t)(beat % 4u + 1u), b);
    text(x, 4, b, C_FG, 1);
    if (tp.playing) {                                   /* play: a triangle */
        int32_t i;
        for (i = 0; i < 7; i++) fill_rect(214 + i, 4 + i / 2, 1, 7 - (i / 2) * 2, C_GREEN);
    } else {
        fill_rect(214, 4, 7, 7, C_DIM);
    }
    if (tp.armed) fill_circle(230, 7, 4, (tp.playing && (ui.frame >> 3 & 1u)) ? C_DARK : C_RED);
    fill_rect(0, 16, TOS_W, 1, C_DARK);
}

static void draw_dial(int32_t cx, uint32_t id, uint16_t col)
{
    char b[12];
    const char *vs;
    int32_t v = param_get(id), r = 15, cy = 203;
    uint32_t span = (uint32_t)(PD[id].max - PD[id].min), a;
    int32_t frac = span ? (int32_t)(((v - PD[id].min) * 1024) / (int32_t)span) : 0;
    a = (uint32_t)(640 - (frac * 3 / 4)) & 1023u;      /* 225 deg .. -45 deg, counter-clockwise from 0 = 3 o'clock */
    text_c(cx, 178, PD[id].name, col, 1);
    circle(cx, cy, r, dim(col, 110));
    circle(cx, cy, r - 1, dim(col, 70));
    fill_circle(cx, cy, 10, dim(col, 60));
    line(cx, cy, cx + (icos(a) * (r - 2) >> 15), cy - (isin(a) * (r - 2) >> 15), col);
    fill_circle(cx + (icos(a) * (r - 1) >> 15), cy - (isin(a) * (r - 1) >> 15), 2, col);
    vs = pval(id, b);
    text_c(cx, 226, vs, C_FG, 1);
}

static void draw_dials(void)
{
    uint32_t k;
    for (k = 0; k < 4u; k++) draw_dial(30 + (int32_t)k * 60, KNOBS[ui.scr][ui.shift][k], KNOB_COLOR[k]);
}

/* ---- SYNTH: the oscillators' waveform, the filter, the scope, the voices */
static int32_t wave_sample(uint32_t wave, uint32_t ph, uint32_t inc)
{
    switch (wave) {
    case 1: return osc_pulse(ph, inc, 0x80000000u);
    case 2: return osc_tri(ph);
    case 3: return osc_sine(ph);
    case 4: return osc_pulse(ph, inc, 0xB0000000u);
    case 6: return (osc_saw(ph, inc) + osc_pulse(ph >> 1, inc >> 1, 0x80000000u)) >> 1;
    default: return osc_saw(ph, inc);
    }
}

static int32_t mod_cutoff(void)                         /* the cutoff the newest voice hears now, 0..127 << 8 */
{
    uint32_t i, best = 0, have = 0;
    int32_t c = param_get(PR_CUT) << 8, env = 0;
    for (i = 0; i < NSVOICE; i++)
        if (sv_view[i].active && sv[i].fv.age >= best) { best = sv[i].fv.age; env = sv_view[i].level; have = 1; }
    if (have) c += (live_snd.fenv * env) >> 7;
    if (live_snd.ldepth && live_snd.ldest == LDEST_CUT) c += (lfo_val * live_snd.ldepth) >> 7;
    return clamp(c, 0, 127 << 8);
}

static void draw_synth(void)
{
    const int32_t wx = 8, wy = 24, ww = 108, wh = 76;
    const int32_t fx = 124, fw = 108;
    uint32_t wave = (uint32_t)param_get(PR_WAVE), det = (uint32_t)param_get(PR_DTN), x;
    int32_t mix = param_get(PR_MIX) * 258, res = param_get(PR_RES), cut = mod_cutoff();
    uint32_t inc = 0xFFFFFFFFu / 54u;                   /* one cycle every 54 px */
    int32_t py = -1, i;
    rect(wx, wy, ww, wh, C_DARK);
    rect(fx, wy, fw, wh, C_DARK);
    fill_rect(wx + 1, wy + wh / 2, ww - 2, 1, C_DARK);
    for (x = 0; x < (uint32_t)ww - 2u; x++) {           /* OSC 1 + OSC 2 (detuned: it drifts across the box) */
        uint32_t ph = x * inc, ph2 = ph + x * (inc >> 10) * det + 0x40000000u;
        int32_t s = mulq15(wave_sample(wave, ph, inc), 32767 - mix) + mulq15(wave_sample(wave, ph2, inc), mix);
        int32_t y = wy + wh / 2 - (s * (wh / 2 - 6) >> 15);
        if (py >= 0) line(wx + 1 + (int32_t)x - 1, py, wx + 1 + (int32_t)x, y, C_BLUE);
        py = y;
    }
    text(wx + 3, wy + 3, N_WAVE[wave], C_BLUE, 1);
    {                                                   /* the filter: a 2-pole response drawn from CUT and RES */
        int32_t ftype = param_get(PR_FTYPE), cx = fx + 4 + (cut * (fw - 8) >> 15), prev = -1;
        for (i = 0; i < fw - 2; i++) {
            int32_t d = (fx + 1 + i) - cx, g, y;   /* distance from the cutoff, px (~ octaves) */
            int32_t peak = res * 22 / 127, ad = d < 0 ? -d : d;
            if (ftype == 0) g = d <= 0 ? 0 : -d * 2;
            else if (ftype == 2) g = d >= 0 ? 0 : d * 2;
            else g = -ad * 2;
            if (ad < 10) g += peak * (10 - ad) / 10;
            y = wy + 30 - g;
            y = clamp(y, wy + 2, wy + wh - 3);
            if (prev >= 0) line(fx + i, prev, fx + 1 + i, y, C_GREEN);
            prev = y;
        }
        fill_rect(cx, wy + 2, 1, wh - 4, dim(C_WHITE, 90));
        text(fx + 3, wy + 3, N_FTYPE[ftype], C_GREEN, 1);
    }
    {                                                   /* the scope: the selected channel, the real audio */
        const int32_t sy = 132, sh = 26;
        uint32_t r = (scope_w + 1u) & (SCOPE_N - 1u), j, start = 0;
        int32_t prev = -1;
        for (j = 0; j + 1u < SCOPE_N / 2u; j++)          /* a rising zero crossing: a steady picture */
            if (scope[(r + j) & (SCOPE_N - 1u)] < 0 && scope[(r + j + 1u) & (SCOPE_N - 1u)] >= 0) { start = j; break; }
        fill_rect(8, sy, 224, 1, C_DARK);
        for (j = 0; j < 112u; j++) {
            int32_t s = scope[(r + start + j) & (SCOPE_N - 1u)];
            int32_t y = clamp(sy - (s * sh >> 14), sy - sh, sy + sh);
            if (prev >= 0) line(8 + (int32_t)j * 2 - 2, prev, 8 + (int32_t)j * 2, y, C_WHITE);
            prev = y;
        }
    }
    for (i = 0; i < (int32_t)NSVOICE; i++) {            /* the voices: size = envelope */
        int32_t cx = 20 + i * 28, lv = sv_view[i].active ? sv_view[i].level : 0;
        uint16_t c = sv_view[i].src == SRC_TAPE ? C_ORANGE : C_WHITE;
        circle(cx, 108, 6, C_DARK);
        if (lv) fill_circle(cx, 108, 1 + (lv * 6 >> 15), c);
    }
}

static void draw_env(void)
{
    const int32_t x0 = 12, y0 = 150, h = 110;
    int32_t a = 8 + live_snd.env[0] * 50 / 127, d = 8 + live_snd.env[1] * 50 / 127, r = 8 + live_snd.env[3] * 50 / 127;
    int32_t s = y0 - live_snd.env[2] * h / 127, top = y0 - h, sx = x0 + a + d, ex = sx + 40;
    uint32_t i, best = 0;
    int32_t lv = -1, st = 0;
    line(x0, y0, x0 + a, top, C_BLUE);
    line(x0 + a, top, sx, s, C_GREEN);
    line(sx, s, ex, s, C_WHITE);
    line(ex, s, ex + r, y0, C_ORANGE);
    fill_rect(x0, y0, ex + r - x0 + 10, 1, C_DARK);
    for (i = 0; i < NSVOICE; i++)                        /* the newest voice: where its envelope is */
        if (sv_view[i].active && sv[i].fv.age >= best) { best = sv[i].fv.age; lv = sv_view[i].level; st = sv[i].stage; }
    if (lv >= 0) {
        int32_t y = y0 - (lv * h >> 15), x = st == 1 ? x0 + a * (lv >> 7) / 256 : st == 2 ? sx + 20 : ex + r / 2;
        fill_circle(x, y, 4, C_YELLOW);
    }
    {
        char b[12];
        text(150, 30, "ENV>CUT", C_DIM, 1);
        text(150, 42, itoa_s(live_snd.fenv, b), C_FG, 2);
    }
}

static void draw_lfo(void)
{
    const int32_t x0 = 12, w = 216, yc = 90, amp = 50;
    int32_t i, prev = -1, depth = live_snd.ldepth, mx;
    for (i = 0; i < w; i++) {
        uint32_t ph = (uint32_t)i * (0xFFFFFFFFu / (uint32_t)(w / 2));
        int32_t s = live_snd.lwave == LFO_SINE ? sine_i(ph) : live_snd.lwave == LFO_SQR ? ((int32_t)ph < 0 ? -32767 : 32767)
                  : live_snd.lwave == LFO_SH ? ((int32_t)((ph >> 28) * 2654435761u) >> 16) : osc_tri(ph);
        int32_t y = yc - ((s * (8 + depth * (amp - 8) / 127)) >> 15);
        if (prev >= 0) line(x0 + i - 1, prev, x0 + i, y, C_BLUE);
        prev = y;
    }
    mx = x0 + (int32_t)((lfo_ph >> 16) * (uint32_t)(w / 2) >> 16);
    fill_circle(mx, yc - ((lfo_val * (8 + depth * (amp - 8) / 127)) >> 15), 4, C_YELLOW);
    text(12, 150, "TO", C_DIM, 1);
    text(30, 150, N_LDEST[live_snd.ldest], C_FG, 1);
}

static void draw_fx(void)
{
    int32_t k, sp = 6 + fx_time * 40 / 127, lvl = 120;
    int32_t fb_ = fx_fdbk * 240, mixh = fx_mix;
    text(12, 26, "DELAY", C_FG, 2);
    fill_rect(12, 150, 216, 1, C_DARK);
    for (k = 0; k < 12; k++) {                          /* the echoes of one hit: spacing = TIME, decay = FEEDBACK */
        int32_t x = 20 + k * sp, h = (lvl * (k ? mixh : 127)) / 127;
        if (x > 226) break;
        fill_rect(x, 150 - h, 4, h, k ? dim(C_GREEN, 140 + (int32_t)fx_tone) : C_WHITE);
        if (k) lvl = (lvl * fb_) >> 15;
    }
    fill_rect(228, 150 - (mx_peak_master * 100 >> 15), 4, mx_peak_master * 100 >> 15, C_ORANGE);
}

/* ---- DRUM: eight pads, lit by their voices */
static void draw_drum(void)
{
    uint32_t k;
    for (k = 0; k < NDRUM; k++) {
        int32_t x = 8 + (int32_t)(k % 4u) * 57, y = 26 + (int32_t)(k / 4u) * 70;
        int32_t lv = dv_view[k];
        uint16_t c = KNOB_COLOR[k % 4u];
        fill_rect(x, y, 51, 60, dim(c, 20 + (lv * 200 >> 15)));
        rect(x, y, 51, 60, k == dr_sel ? C_ORANGE : C_DARK);
        if (k == dr_sel) rect(x + 1, y + 1, 49, 58, C_ORANGE);
        text_c(x + 25, y + 46, DRUM_NAME[k], lv > 6000 ? C_BG : C_FG, 1);
        fill_circle(x + 25, y + 22, 4 + (lv * 10 >> 15), lv ? C_FG : C_DARK);
    }
}

/* ---- TAPE: the reels, four tracks, the playhead, the history */
static const uint16_t TRACK_COLOR[NTRACK] = {C_BLUE, C_GREEN, C_WHITE, C_ORANGE};

static void draw_reel(int32_t cx, int32_t cy, uint32_t ang, int32_t fill)
{
    int32_t k;
    circle(cx, cy, 24, C_DIM);
    circle(cx, cy, 10 + fill, C_DARK);
    fill_circle(cx, cy, 4, C_FG);
    for (k = 0; k < 3; k++) {
        uint32_t a = (ang + (uint32_t)k * 341u) & 1023u;
        line(cx + (icos(a) * 5 >> 15), cy - (isin(a) * 5 >> 15), cx + (icos(a) * 20 >> 15), cy - (isin(a) * 20 >> 15), C_FG);
    }
}

static void draw_tape(void)
{
    const int32_t lx = 8, lw = 224, ly = 80, lh = 20;
    uint32_t len = tp.loop ? tp.loop_end : TAPE_MAX_TICKS, k, i, pos = tp.pos;
    int32_t px;
    uint32_t ang = (pos >> 3) & 1023u;                   /* the reels turn with the tape */
    int32_t fl = (int32_t)(pos * 12u / (len ? len : 1u));
    char b[16];
    draw_reel(64, 46, 1023u - ang, 12 - fl);
    draw_reel(176, 46, 1023u - ang, fl);
    line(64, 70, 176, 70, C_DIM);
    for (k = 0; k < NTRACK; k++) {
        int32_t y = ly + (int32_t)k * lh;
        const ttrack_t *t = &tt[k];
        fill_rect(lx, y + 1, lw, lh - 2, k == tp.track ? C_DARK : C_BG);
        if (k == tp.track) rect(lx - 1, y, lw + 2, lh, tp.armed ? C_RED : TRACK_COLOR[k]);
        for (i = 0; i < t->n; i++) {
            const tev_t *e = &t->ev[i];
            int32_t x, w, yy;
            if (!tev_visible(e) || e->tick >= len) continue;
            x = lx + (int32_t)((uint64_t)e->tick * (uint32_t)lw / len);
            w = (int32_t)((uint64_t)e->dur * (uint32_t)lw / len);
            if (w < 1) w = 1;
            if (x + w > lx + lw) w = lx + lw - x;
            yy = (e->vel & TEV_DRUM) ? y + 15 - (e->note & 7u) * 2 : y + 16 - (int32_t)(e->note % 24u) * 14 / 24;
            fill_rect(x, yy, w, 2, TRACK_COLOR[k]);
        }
        b[0] = (char)('1' + k); b[1] = 0;
        text(lx + lw - 8, y + 7, b, k == tp.track ? C_FG : C_DIM, 1);
    }
    px = lx + (int32_t)((uint64_t)pos * (uint32_t)lw / (len ? len : 1u));
    fill_rect(px, ly - 2, 1, NTRACK * lh + 4, tp.armed ? C_RED : C_YELLOW);
    {                                                   /* the history: levels to undo / redo */
        int32_t x = text(8, 164, "UNDO ", C_DIM, 1);
        x = text(x, 164, itoa_s(tp.cur - tp.base, b), C_FG, 1);
        x = text(x, 164, "  REDO ", C_DIM, 1);
        text(x, 164, itoa_s(tp.top - tp.cur, b), C_FG, 1);
        if (tp.armed) text(196, 164, "REC", (ui.frame >> 3 & 1u) ? C_RED : C_DIM, 1);
    }
}

/* ---- MIXER: four strips and the master, with their meters */
static void draw_mixer(void)
{
    uint32_t c;
    for (c = 0; c < NTRACK; c++) {
        int32_t x = 22 + (int32_t)c * 48, top = 30, h = 120;
        int32_t fy = top + h - (mx_vol[c] * h / 127), m = mx_peak[c] * h >> 15;
        char b[4];
        fill_rect(x, top, 2, h, C_DARK);
        fill_rect(x + 8, top + h - m, 5, m, mx_mute[c] ? C_DIM : TRACK_COLOR[c]);
        fill_rect(x - 7, fy - 3, 16, 7, TRACK_COLOR[c]);
        fill_rect(x - 8 + 8 + mx_pan[c] / 8, top - 8, 3, 3, C_FG);
        b[0] = (char)('1' + c); b[1] = 0;
        text(x - 2, top + h + 6, mx_mute[c] ? "M" : b, mx_mute[c] ? C_RED : C_FG, 1);
    }
    {
        int32_t m = mx_peak_master * 120 >> 15;
        fill_rect(216, 30, 6, 120, C_DARK);
        fill_rect(216, 150 - m, 6, m, m > 110 ? C_RED : C_FG);
        text(206, 156, "MST", C_DIM, 1);
    }
    if (cpu_permille) {                                 /* the audio load, where the platform measures it (FM1) */
        char b[12];
        int32_t x = text(150, 164, "CPU ", C_DIM, 1);
        x = text(x, 164, itoa_s((int32_t)(cpu_permille / 10u), b), cpu_permille > 700u ? C_RED : C_FG, 1);
        text(x, 164, "%", C_DIM, 1);
    }
}

static void draw_popup(void)
{
    static const char *const P[] = {"", "UNDO", "REDO", "NO UNDO", "NO REDO"};
    if (!ui.popup) return;
    fill_rect(40, 84, 160, 52, C_BG);
    rect(40, 84, 160, 52, ui.popup <= 2 ? C_FG : C_DIM);
    text_c(120, 100, P[ui.popup], ui.popup <= 2 ? C_FG : C_DIM, ui.popup <= 2 ? 3 : 2);
}

static void render_frame(void)
{
    fb_clear(C_BG);
    draw_header();
    switch (ui.scr) {
    case SCR_SYNTH: draw_synth(); break;
    case SCR_ENV: draw_env(); break;
    case SCR_LFO: draw_lfo(); break;
    case SCR_FX: draw_fx(); break;
    case SCR_DRUM: draw_drum(); break;
    case SCR_TAPE: draw_tape(); break;
    default: draw_mixer(); break;
    }
    fill_rect(0, 171, TOS_W, 1, C_DARK);
    draw_dials();
    draw_popup();
    ui.frame++;
}
