/* SPDX-License-Identifier: GPL-3.0-only */
/* SYNTH: a voice manager around Felucca's ANALOG engine (third_party/felucca/eng_analog.c, used unchanged).
 * The engine renders a voice from the sound's parameters and a per-block modulation (vmod_t); what feeds it
 * here (ADSR, LFO, voice allocation) is FM1 Tape OS's own code. Runs on the audio side only. */

#define SRC_LIVE 0u
#define SRC_TAPE 1u

typedef struct {
    voice_t fv;                  /* Felucca's voice: oscillator phases, filter state (eng_analog.c owns s[]) */
    sound_t *snd;
    uint8_t note, vel, track, src;
    uint8_t stage;               /* 0 free, 1 attack, 2 decay/sustain, 3 release */
    int32_t env;                 /* Q24 */
    int32_t amp;                 /* the amplitude at the end of the last block, Q15 */
} svoice_t;

static svoice_t sv[NSVOICE];
static uint32_t sv_age;
static sound_t live_snd;         /* what the keys play */
static sound_t trk_snd[NTRACK];  /* what each tape track plays (taken from live_snd when a take starts) */
static uint32_t lfo_ph;
static int32_t lfo_val;          /* Q15 */
static int32_t lfo_sh = 0x2468ACE;
static vview_t sv_view[NSVOICE]; /* for the screen */

static void sound_preset(sound_t *s, uint32_t i)
{
    const preset_t *p;
    uint32_t k;
    if (i >= ENG_ANALOG.npresets) i = 0;
    p = &ENG_ANALOG.presets[i];
    for (k = 0; k < P_COUNT; k++) s->t.p[k] = 0;
    for (k = 0; k < 8u; k++) s->t.p[P_E0 + k] = p->e[k];
    for (k = 0; k < 4u; k++) s->env[k] = p->env[k];
    s->fenv = p->fenv;
    s->preset = (uint8_t)i;
    s->lrate = 60; s->ldepth = 0; s->ldest = LDEST_CUT; s->lwave = LFO_TRI;
}

static void synth_init(void)
{
    uint32_t i;
    sound_preset(&live_snd, 0);
    for (i = 0; i < NTRACK; i++) trk_snd[i] = live_snd;
    for (i = 0; i < NSVOICE; i++) sv[i].stage = 0;
}

static void synth_note_on(uint32_t track, uint32_t note, uint32_t vel, uint32_t src, sound_t *snd)
{
    uint32_t i, pick = 0, best = 0xFFFFFFFFu;
    for (i = 0; i < NSVOICE; i++) {          /* a free voice, else the oldest released, else the oldest */
        uint32_t score = sv[i].stage == 0 ? 0u : sv[i].stage == 3 ? 0x40000000u + (sv[i].fv.age & 0x3FFFFFFFu)
                                                                  : 0x80000000u + (sv[i].fv.age & 0x3FFFFFFFu);
        if (score < best) { best = score; pick = i; }
    }
    {
        svoice_t *v = &sv[pick];
        v->fv.age = ++sv_age;
        v->fv.pitch16 = (int32_t)note * 16;
        v->fv.pitch_cur = v->fv.pitch16;
        if (v->stage == 0) v->amp = 0;
        v->snd = snd;
        v->note = (uint8_t)note;
        v->vel = (uint8_t)(vel > 127u ? 127u : vel);
        v->track = (uint8_t)track;
        v->src = (uint8_t)src;
        v->stage = 1;
        if (v->env < 0) v->env = 0;
        ENG_ANALOG.note_on(&snd->t, &v->fv);
    }
}

static void synth_note_off(uint32_t track, uint32_t note, uint32_t src)
{
    uint32_t i;
    for (i = 0; i < NSVOICE; i++)
        if (sv[i].stage && sv[i].stage != 3 && sv[i].note == note && sv[i].track == track && sv[i].src == src)
            sv[i].stage = 3;
}

static void synth_release_all(uint32_t src_mask)   /* bit SRC_*: release those voices */
{
    uint32_t i;
    for (i = 0; i < NSVOICE; i++)
        if (sv[i].stage && sv[i].stage != 3 && (src_mask >> sv[i].src & 1u))
            sv[i].stage = 3;
}

/* ADSR, once per block: Felucca's generated curves (ENV_LIN: attack steps, ENV_EXP: decay/release factors) */
static void env_tick(svoice_t *v)
{
    const uint8_t *e = v->snd->env;
    switch (v->stage) {
    case 1:
        v->env += (int32_t)ENV_LIN[e[0]];
        if (v->env >= (1 << 24)) { v->env = 1 << 24; v->stage = 2; }
        break;
    case 2: {
        int32_t sus = (int32_t)e[2] * 132104;            /* 127 -> 1.0 in Q24 */
        v->env += (int32_t)(((int64_t)(sus - v->env) * ENV_EXP[e[1]]) >> 16);
        break;
    }
    case 3:
        v->env -= (int32_t)(((int64_t)v->env * ENV_EXP[e[3]]) >> 16);
        if (v->env < 2048) { v->env = 0; v->stage = 0; }
        break;
    default:
        break;
    }
}

static void lfo_tick(const sound_t *s)
{
    uint32_t ph;
    lfo_ph += LFO_INC[s->lrate & 127u];
    ph = lfo_ph;
    switch (s->lwave) {
    case LFO_SINE: lfo_val = sine_i(ph); break;
    case LFO_SQR: lfo_val = (int32_t)ph < 0 ? -32767 : 32767; break;
    case LFO_SH:
        if (ph < LFO_INC[s->lrate & 127u]) lfo_val = ((int32_t)(noise32(&lfo_sh) >> 17) - 16384) * 2;
        break;
    default: lfo_val = osc_tri(ph); break;
    }
}

/* render every sounding voice of the block into its track's bus (mono, CTL samples) */
static void synth_block(int32_t bus[NTRACK][CTL])
{
    uint32_t i;
    lfo_tick(&live_snd);
    for (i = 0; i < NSVOICE; i++) {
        svoice_t *v = &sv[i];
        const sound_t *s = v->snd;
        vmod_t m;
        int32_t envq15, a1, lfo, depth;
        if (!v->stage) { sv_view[i].active = 0; continue; }
        env_tick(v);
        envq15 = v->env >> 9;
        lfo = lfo_val;
        depth = s->ldepth;
        m.pitch16 = v->fv.pitch16;
        m.cutoff = (s->fenv * envq15) >> 7;
        m.shape = 64 << 8;
        if (depth) {
            switch (s->ldest) {
            case LDEST_PITCH: m.pitch16 += (lfo * depth) >> 15; break;
            case LDEST_SHAPE: m.shape += (lfo * depth) >> 8; break;
            case LDEST_AMP: break;
            default: m.cutoff += (lfo * depth) >> 7; break;
            }
        }
        m.pitch16 = clamp(m.pitch16, 0, 2047);
        m.inc = pitch_inc((uint32_t)m.pitch16);
        m.fine = 0;
        m.envq15 = envq15;
        a1 = (envq15 * (int32_t)v->vel * 258) >> 15;
        if (depth && s->ldest == LDEST_AMP)
            a1 -= (a1 * (((lfo + 32767) >> 1) * depth >> 7)) >> 15;
        m.amp0 = v->amp;
        m.amp1 = a1;
        v->amp = a1;
        ENG_ANALOG.render((track_t *)&s->t, &v->fv, bus[v->track & 3u], CTL, &m);
        sv_view[i].active = 1;
        sv_view[i].note = v->note;
        sv_view[i].track = v->track;
        sv_view[i].src = v->src;
        sv_view[i].level = envq15;
    }
}
