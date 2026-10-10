/* SPDX-License-Identifier: GPL-3.0-only */
/* The audio side: the command queue from the main loop, the parameter registry, and tos_audio(), which runs
 * the tape clock, the voices and the mixer in control blocks of CTL (32) frames. */

/* ---- commands: written by the main loop only, read by the audio side only */
#define CMDQ_N 128u
static cmd_t cmdq[CMDQ_N];
static volatile uint32_t cmdq_w, cmdq_r;
static uint32_t cmd_dropped;

static void cmd_push(uint32_t type, uint32_t a, int32_t b)
{
    uint32_t w = cmdq_w;
    if (w - cmdq_r >= CMDQ_N) { cmd_dropped++; return; }
    cmdq[w % CMDQ_N].type = (uint8_t)type;
    cmdq[w % CMDQ_N].a = (uint8_t)a;
    cmdq[w % CMDQ_N].b = (int16_t)b;
    __asm__ volatile("" ::: "memory");          /* the slot before the index (as Felucca's RING_PUBLISH) */
    cmdq_w = w + 1u;
}

/* ---- parameters */
static const char *const N_WAVE[] = {"SAW", "SQUARE", "TRI", "SINE", "PWM", "SYNC", "SUB"};
static const char *const N_FTYPE[] = {"LOWPASS", "BANDPASS", "HIGHPASS"};
static const char *const N_LDEST[] = {"CUTOFF", "PITCH", "AMP", "SHAPE"};
static const char *const N_LWAVE[] = {"TRI", "SINE", "SQUARE", "S+H"};
static const char *const N_ONOFF[] = {"OFF", "ON"};

static const pdesc_t PD[NPARAM] = {
    [PR_WAVE] = {"WAVE", 0, 6, N_WAVE}, [PR_DTN] = {"DETUNE", 0, 127, 0}, [PR_MIX] = {"MIX", 0, 127, 0},
    [PR_NOIS] = {"NOISE", 0, 127, 0}, [PR_CUT] = {"CUTOFF", 0, 127, 0}, [PR_RES] = {"RESO", 0, 127, 0},
    [PR_DRV] = {"DRIVE", 0, 127, 0}, [PR_KTR] = {"KEYTRK", 0, 127, 0}, [PR_FTYPE] = {"FILTER", 0, 2, N_FTYPE},
    [PR_ATK] = {"ATTACK", 0, 127, 0}, [PR_DEC] = {"DECAY", 0, 127, 0}, [PR_SUS] = {"SUSTAIN", 0, 127, 0},
    [PR_REL] = {"RELEASE", 0, 127, 0}, [PR_FENV] = {"ENV>CUT", -64, 63, 0},
    [PR_LRATE] = {"RATE", 0, 127, 0}, [PR_LDEPTH] = {"DEPTH", 0, 127, 0}, [PR_LDEST] = {"DEST", 0, 3, N_LDEST},
    [PR_LWAVE] = {"SHAPE", 0, 3, N_LWAVE}, [PR_PRESET] = {"PRESET", 0, 11, 0},
    [PR_DRSEL] = {"SOUND", 0, NDRUM - 1, DRUM_NAME}, [PR_DTUNE] = {"TUNE", 0, 127, 0},
    [PR_DDECAY] = {"DECAY", 0, 127, 0}, [PR_DTONE] = {"TONE", 0, 127, 0}, [PR_DLEVEL] = {"LEVEL", 0, 127, 0},
    [PR_TRACK] = {"TRACK", 0, 3, 0}, [PR_BARS] = {"BARS", 1, 16, 0}, [PR_BPM] = {"TEMPO", 60, 200, 0},
    [PR_LOOP] = {"LOOP", 0, 1, N_ONOFF},
    [PR_VOL1] = {"VOL 1", 0, 127, 0}, [PR_VOL2] = {"VOL 2", 0, 127, 0}, [PR_VOL3] = {"VOL 3", 0, 127, 0},
    [PR_VOL4] = {"VOL 4", 0, 127, 0},
    [PR_PAN1] = {"PAN 1", -64, 63, 0}, [PR_PAN2] = {"PAN 2", -64, 63, 0}, [PR_PAN3] = {"PAN 3", -64, 63, 0},
    [PR_PAN4] = {"PAN 4", -64, 63, 0},
    [PR_MUTE1] = {"MUTE 1", 0, 1, N_ONOFF}, [PR_MUTE2] = {"MUTE 2", 0, 1, N_ONOFF},
    [PR_MUTE3] = {"MUTE 3", 0, 1, N_ONOFF}, [PR_MUTE4] = {"MUTE 4", 0, 1, N_ONOFF},
    [PR_MASTER] = {"MASTER", 0, 127, 0},
    [PR_FXTIME] = {"TIME", 0, 127, 0}, [PR_FXFDBK] = {"FEEDBK", 0, 127, 0}, [PR_FXMIX] = {"MIX", 0, 127, 0},
    [PR_FXTONE] = {"TONE", 0, 127, 0},
};

static uint8_t dr_sel;           /* the drum sound the DRUM knobs edit (the last one played) */

static int32_t param_get(uint32_t id)
{
    const int16_t *p = live_snd.t.p;
    switch (id) {
    case PR_WAVE: case PR_DTN: case PR_MIX: case PR_NOIS: case PR_CUT: case PR_RES: case PR_DRV: case PR_KTR:
        return p[P_E0 + (id - PR_WAVE)];
    case PR_FTYPE: return p[P_FTYPE];
    case PR_ATK: case PR_DEC: case PR_SUS: case PR_REL: return live_snd.env[id - PR_ATK];
    case PR_FENV: return live_snd.fenv;
    case PR_LRATE: return live_snd.lrate;
    case PR_LDEPTH: return live_snd.ldepth;
    case PR_LDEST: return live_snd.ldest;
    case PR_LWAVE: return live_snd.lwave;
    case PR_PRESET: return live_snd.preset;
    case PR_DRSEL: return dr_sel;
    case PR_DTUNE: return dsnd[dr_sel].tune;
    case PR_DDECAY: return dsnd[dr_sel].decay;
    case PR_DTONE: return dsnd[dr_sel].tone;
    case PR_DLEVEL: return dsnd[dr_sel].level;
    case PR_TRACK: return tp.track;
    case PR_BARS: return tp.bars;
    case PR_BPM: return tp.bpm;
    case PR_LOOP: return tp.loop;
    case PR_VOL1: case PR_VOL2: case PR_VOL3: case PR_VOL4: return mx_vol[id - PR_VOL1];
    case PR_PAN1: case PR_PAN2: case PR_PAN3: case PR_PAN4: return mx_pan[id - PR_PAN1];
    case PR_MUTE1: case PR_MUTE2: case PR_MUTE3: case PR_MUTE4: return mx_mute[id - PR_MUTE1];
    case PR_MASTER: return mx_master;
    case PR_FXTIME: return fx_time;
    case PR_FXFDBK: return fx_fdbk;
    case PR_FXMIX: return fx_mix;
    case PR_FXTONE: return fx_tone;
    default: return 0;
    }
}

static void param_set(uint32_t id, int32_t v)
{
    int16_t *p = live_snd.t.p;
    if (id >= NPARAM) return;
    v = clamp(v, PD[id].min, PD[id].max);
    switch (id) {
    case PR_WAVE: case PR_DTN: case PR_MIX: case PR_NOIS: case PR_CUT: case PR_RES: case PR_DRV: case PR_KTR:
        p[P_E0 + (id - PR_WAVE)] = (int16_t)v; break;
    case PR_FTYPE: p[P_FTYPE] = (int16_t)v; break;
    case PR_ATK: case PR_DEC: case PR_SUS: case PR_REL: live_snd.env[id - PR_ATK] = (uint8_t)v; break;
    case PR_FENV: live_snd.fenv = (int8_t)v; break;
    case PR_LRATE: live_snd.lrate = (uint8_t)v; break;
    case PR_LDEPTH: live_snd.ldepth = (uint8_t)v; break;
    case PR_LDEST: live_snd.ldest = (uint8_t)v; break;
    case PR_LWAVE: live_snd.lwave = (uint8_t)v; break;
    case PR_PRESET: {
        sound_t s = live_snd;
        sound_preset(&s, (uint32_t)v);
        s.lrate = live_snd.lrate; s.ldepth = live_snd.ldepth; s.ldest = live_snd.ldest; s.lwave = live_snd.lwave;
        live_snd = s;
        break;
    }
    case PR_DRSEL: dr_sel = (uint8_t)v; break;
    case PR_DTUNE: dsnd[dr_sel].tune = (uint8_t)v; break;
    case PR_DDECAY: dsnd[dr_sel].decay = (uint8_t)v; break;
    case PR_DTONE: dsnd[dr_sel].tone = (uint8_t)v; break;
    case PR_DLEVEL: dsnd[dr_sel].level = (uint8_t)v; break;
    case PR_TRACK: tape_set_track((uint32_t)v); break;
    case PR_BARS: tape_set_loop((uint32_t)v, tp.bpm, tp.loop); break;
    case PR_BPM: tape_set_loop(tp.bars, (uint32_t)v, tp.loop); break;
    case PR_LOOP: tape_set_loop(tp.bars, tp.bpm, (uint32_t)v); break;
    case PR_VOL1: case PR_VOL2: case PR_VOL3: case PR_VOL4: mx_vol[id - PR_VOL1] = (uint8_t)v; break;
    case PR_PAN1: case PR_PAN2: case PR_PAN3: case PR_PAN4: mx_pan[id - PR_PAN1] = (int8_t)v; break;
    case PR_MUTE1: case PR_MUTE2: case PR_MUTE3: case PR_MUTE4: mx_mute[id - PR_MUTE1] = (uint8_t)v; break;
    case PR_MASTER: mx_master = (uint8_t)v; break;
    case PR_FXTIME: fx_time = (uint8_t)v; break;
    case PR_FXFDBK: fx_fdbk = (uint8_t)v; break;
    case PR_FXMIX: fx_mix = (uint8_t)v; break;
    case PR_FXTONE: fx_tone = (uint8_t)v; break;
    default: break;
    }
}

/* ---- what the audio side reports to the screen */
static volatile uint8_t ev_popup;       /* 1 UNDO, 2 REDO, 3 nothing to undo, 4 nothing to redo; the UI clears it */
static uint32_t audio_frames;

static void cmd_apply(const cmd_t *c)
{
    switch (c->type) {
    case CMD_NOTE_ON:
        synth_note_on(tp.track, c->a, (uint32_t)c->b, SRC_LIVE, &live_snd);
        tape_rec_start_note(c->a, 0);
        break;
    case CMD_NOTE_OFF:
        synth_note_off(tp.track, c->a, SRC_LIVE);
        tape_rec_end_note(c->a, 0);
        break;
    case CMD_DRUM_ON:
        dr_sel = c->a < NDRUM ? c->a : 0;
        drum_hit(c->a, tp.track, (uint32_t)c->b);
        tape_rec_start_note(c->a, 1);
        break;
    case CMD_DRUM_OFF:
        tape_rec_end_note(c->a, 1);
        break;
    case CMD_PARAM:
        param_set(c->a, param_get(c->a) + c->b);
        break;
    case CMD_PLAY: tape_play(!tp.playing); break;
    case CMD_REC: tape_arm(!tp.armed); break;
    case CMD_REWIND: tape_seek(0); break;
    case CMD_SEEK: {
        int32_t t = (int32_t)tp.pos + c->b * (int32_t)(ticks_per_beat_x10() / 10u);
        tape_seek(t < 0 ? 0u : (uint32_t)t);
        break;
    }
    case CMD_UNDO: ev_popup = tape_undo() ? 1 : 3; break;
    case CMD_REDO: ev_popup = tape_redo() ? 2 : 4; break;
    case CMD_ERASE_TRACK: tape_erase_track(tp.track); break;
    case CMD_PANIC: synth_release_all(3u); tape_all_off(); break;
    default: break;
    }
}

static int32_t bus[NTRACK][CTL];
static int32_t part[2u * CTL];          /* a block computed but not yet handed out (n not a multiple of CTL) */
static uint32_t part_n, part_r;

static void engine_init(void)
{
    synth_init();
    drum_init();
    tape_init();
    cmdq_w = cmdq_r = 0;
    part_n = part_r = 0;
}

static void engine_block(int32_t *out)  /* one control block: CTL stereo frames, added to out */
{
    uint32_t k, i;
    while (cmdq_r != cmdq_w) {
        cmd_apply(&cmdq[cmdq_r % CMDQ_N]);
        cmdq_r++;
    }
    tape_tick();
    for (k = 0; k < NTRACK; k++)
        for (i = 0; i < CTL; i++) bus[k][i] = 0;
    synth_block(bus);
    drum_block(bus);
    mix_block(bus, out, tp.track);
    audio_frames += CTL;
}

void tos_audio(int32_t *out, uint32_t n)
{
    while (n) {
        uint32_t i, take;
        if (part_r == part_n) {
            for (i = 0; i < 2u * CTL; i++) part[i] = 0;
            engine_block(part);
            part_n = CTL;
            part_r = 0;
        }
        take = part_n - part_r < n ? part_n - part_r : n;
        for (i = 0; i < take; i++) {
            out[0] += part[2u * part_r];
            out[1] += part[2u * part_r + 1u];
            out += 2;
            part_r++;
        }
        n -= take;
    }
}
