/* SPDX-License-Identifier: GPL-3.0-only */
/* DRUM: eight synthesised drum sounds (FM1 Tape OS's own, fixed point). One voice per sound: a new hit
 * restarts it. Uses Felucca's DSP helpers (oscillators, noise) and generated decay table (DECAY_K). */

enum { DR_KICK, DR_SNARE, DR_CHAT, DR_OHAT, DR_CLAP, DR_LTOM, DR_HTOM, DR_BELL };
static const char *const DRUM_NAME[NDRUM] = {"KICK", "SNARE", "C.HAT", "O.HAT", "CLAP", "LO TOM", "HI TOM", "BELL"};
static const uint8_t DRUM_NOTE[NDRUM] = {29, 50, 0, 0, 0, 43, 52, 77};   /* base pitch (MIDI) of the tonal parts */

typedef struct {
    uint8_t on, track;
    uint32_t ph, ph2, t;
    int32_t amp, pamp;           /* body and pitch-sweep envelopes, Q15 */
    int32_t nst, lp, hp;
} dvoice_t;

static dsound_t dsnd[NDRUM];
static dvoice_t dv[NDRUM];
static int32_t dv_view[NDRUM];   /* level for the screen, Q15 */

static void drum_init(void)
{
    static const dsound_t DEF[NDRUM] = {
        {64, 70, 40, 120}, {64, 50, 70, 105}, {64, 25, 90, 80}, {64, 75, 90, 75},
        {64, 55, 60, 100}, {64, 70, 30, 105}, {64, 60, 30, 100}, {64, 45, 64, 70}};
    uint32_t i;
    for (i = 0; i < NDRUM; i++) {
        dsnd[i] = DEF[i];
        dv[i].on = 0;
        dv[i].nst = 0x1357 + (int32_t)i * 7919;
    }
}

static void drum_hit(uint32_t k, uint32_t track, uint32_t vel)
{
    dvoice_t *v;
    if (k >= NDRUM) return;
    v = &dv[k];
    v->on = 1;
    v->track = (uint8_t)track;
    v->t = 0;
    v->ph = v->ph2 = 0;
    v->amp = (int32_t)(vel > 127u ? 127u : vel) * 258;
    v->pamp = 32767;
    v->lp = v->hp = 0;
}

static int32_t hp1(int32_t x, int32_t *lp, int32_t k) { *lp += ((x - *lp) * k) >> 15; return x - *lp; }

static void drum_block(int32_t bus[NTRACK][CTL])
{
    uint32_t k, i;
    for (k = 0; k < NDRUM; k++) {
        dvoice_t *v = &dv[k];
        const dsound_t *d = &dsnd[k];
        int32_t *out;
        uint32_t dk, base16;
        int32_t lvl;
        if (!v->on) { dv_view[k] = 0; continue; }
        out = bus[v->track & 3u];
        dk = DECAY_K[d->decay & 127u];
        base16 = (uint32_t)DRUM_NOTE[k] * 16u + ((uint32_t)d->tune - 64u) * 4u;   /* TUNE: +-16 semitones */
        lvl = (int32_t)d->level * 258;
        for (i = 0; i < CTL; i++, v->t++) {
            int32_t s, n, tone = d->tone * 258;
            switch (k) {
            case DR_KICK: case DR_LTOM: case DR_HTOM: {
                uint32_t sweep = k == DR_KICK ? 640u : 160u;              /* pitch drop: 40 / 10 semitones */
                uint32_t p16 = base16 + ((uint32_t)v->pamp * sweep >> 15);
                v->ph += pitch_inc(p16 > 2047u ? 2047u : p16);
                v->pamp = (v->pamp * (k == DR_KICK ? 64880 : 65200)) >> 16;
                s = sine_i(v->ph);
                if (k == DR_KICK && v->t < 64u)                           /* the click, TONE loud */
                    s += mulq15((int32_t)(noise32(&v->nst) >> 17) - 16384, tone >> 1);
                break;
            }
            case DR_SNARE:
                v->ph += pitch_inc(base16);
                n = hp1((int32_t)(noise32(&v->nst) >> 16) - 32768, &v->lp, 9000);
                s = mulq15(sine_i(v->ph), 32767 - tone) + mulq15(n, tone);
                break;
            case DR_CHAT: case DR_OHAT:
                n = (int32_t)(noise32(&v->nst) >> 16) - 32768;
                s = hp1(n, &v->lp, 20000 - tone * 80);                    /* TONE: brighter */
                break;
            case DR_CLAP: {
                uint32_t t = v->t;
                n = hp1((int32_t)(noise32(&v->nst) >> 16) - 32768, &v->lp, 12000);
                s = n;
                if (t < 1323u) {                                          /* three bursts, 10 ms apart */
                    int32_t b = 32767 - (int32_t)(t % 441u) * 70;
                    s = mulq15(n, b < 0 ? 0 : b);
                    v->amp = v->amp < 26000 ? 26000 : v->amp;
                }
                (void)tone;
                break;
            }
            default:                                                       /* BELL: two squares, 808 style */
                v->ph += pitch_inc(base16);
                v->ph2 += pitch_inc(base16 + 115u);
                s = (((int32_t)v->ph < 0 ? 12000 : -12000) + ((int32_t)v->ph2 < 0 ? 12000 : -12000));
                s = s - hp1(s, &v->hp, 1500 + tone * 20);
                break;
            }
            v->amp = (int32_t)(((uint32_t)v->amp * dk) >> 16);
            out[i] += mulq15(mulq15(s, v->amp), lvl) >> 1;
        }
        dv_view[k] = v->amp;
        if (v->amp < 8) v->on = 0;
    }
}
