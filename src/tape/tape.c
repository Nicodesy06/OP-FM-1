/* SPDX-License-Identifier: GPL-3.0-only */
/* TAPE: four tracks of note events on a tape that runs with the audio clock (one tick = one control block,
 * 32 samples), with play, record (overdub), loop and a 7-level undo / redo of recordings.
 *
 * This is an EventTape: it records notes and drum hits, not sound. The OP-1's tape records PCM; why the FM1
 * cannot (512 KiB of RAM: ~4 s of mono 16-bit audio) is in docs/ARCHITECTURE.md, and the API keeps the
 * transport and history apart from the event storage so an AudioTape backend could sit beside it.
 *
 * Undo / redo (after the behaviour of Maiko Pruett's OS #248 for the OP-1: hold TAPE + left / right,
 * up to 7 recordings back, punch-ins, overdubs and loop recordings undo exactly, playing or stopped)
 * is a history of operations, not copies of the tape: every event carries the serial of the operation that
 * added it (add) and of the one that deleted it (del). Operations 1..top exist, 1..cur are applied:
 *     visible(e) = e.add <= cur && !(e.del && e.del <= cur)
 * undo = cur - 1, redo = cur + 1, a new operation after an undo first drops what cannot come back (events
 * added after cur, deletions after cur). Operations at or below base are permanent (more than 7 back).
 * The cost is 4 bytes per event, whatever the number of levels.
 *
 * Runs on the audio side; the screen reads it as it is. */

typedef struct {
    tev_t ev[TAPE_MAXEV];
    uint16_t n;                  /* events stored (visible or not) */
    uint16_t cur;                /* playback cursor: first event with tick >= tp.pos */
} ttrack_t;

static ttrack_t tt[NTRACK] TOS_POOL;

static struct {
    uint32_t pos;                /* next tick to play */
    uint32_t loop_end;           /* ticks; the loop is 0..loop_end */
    uint8_t playing, armed, loop;
    uint8_t track;               /* the selected track: live play and recording go to it */
    uint8_t bars, bpm;
    uint16_t base, cur, top;     /* history serials */
    uint16_t take;               /* serial of the take being recorded, 0 = none yet */
    hop_t hop[HIST_LEVELS + 1u]; /* the operations, indexed by serial % 8 */
    /* notes being recorded: started, not yet released */
    struct { uint8_t on, note, drum; uint32_t start; } rec[16];
    /* tape notes sounding: their end */
    struct { uint8_t on, note, drum; uint32_t off; } play[NTRACK][16];
    uint32_t events_dropped;     /* a track was full */
} tp;

#define TAPE_MAX_TICKS 496125u   /* 6 minutes, as the OP-1's tape */

static uint32_t ticks_per_beat_x10(void) { return 826875u / tp.bpm; }   /* 44100 * 60 / 32 = 82687.5 */
static uint32_t loop_ticks(void) { return (uint32_t)tp.bars * 4u * 826875u / ((uint32_t)tp.bpm * 10u); }

static int tev_visible(const tev_t *e)
{
    return e->add <= tp.cur && !(e->del && e->del <= tp.cur);
}

static uint16_t lower_bound(const ttrack_t *t, uint32_t tick)
{
    uint32_t lo = 0, hi = t->n;
    while (lo < hi) {
        uint32_t mid = (lo + hi) >> 1;
        if (t->ev[mid].tick < tick) lo = mid + 1u; else hi = mid;
    }
    return (uint16_t)lo;
}

static void tape_recursor(void)
{
    uint32_t k;
    for (k = 0; k < NTRACK; k++) tt[k].cur = lower_bound(&tt[k], tp.pos);
}

static void tape_init(void)
{
    uint32_t k;
    for (k = 0; k < NTRACK; k++) tt[k].n = tt[k].cur = 0;
    tp.pos = 0;
    tp.playing = tp.armed = 0;
    tp.loop = 1;
    tp.track = 0;
    tp.bars = 4;
    tp.bpm = 120;
    tp.loop_end = loop_ticks();
    tp.base = tp.cur = tp.top = 0;
    tp.take = 0;
}

/* ---- tape notes that sound: start them, end them */
static void tape_voice_on(uint32_t k, uint32_t note, uint32_t drum, uint32_t vel, uint32_t off)
{
    uint32_t i;
    if (drum) drum_hit(note, k, vel);
    else synth_note_on(k, note, vel, SRC_TAPE, &trk_snd[k]);
    for (i = 0; i < 16u; i++)
        if (!tp.play[k][i].on) {
            tp.play[k][i].on = 1;
            tp.play[k][i].note = (uint8_t)note;
            tp.play[k][i].drum = (uint8_t)drum;
            tp.play[k][i].off = off;
            return;
        }
}

static void tape_all_off(void)
{
    uint32_t k, i;
    for (k = 0; k < NTRACK; k++)
        for (i = 0; i < 16u; i++) tp.play[k][i].on = 0;
    synth_release_all(1u << SRC_TAPE);
}

/* ---- history */
static void hist_drop_future(void)    /* a new operation after an undo: what was undone is gone */
{
    uint32_t k, i, j;
    for (k = 0; k < NTRACK; k++) {
        ttrack_t *t = &tt[k];
        for (i = j = 0; i < t->n; i++) {
            tev_t e = t->ev[i];
            if (e.add > tp.cur) continue;
            if (e.del > tp.cur) e.del = 0;
            t->ev[j++] = e;
        }
        t->n = (uint16_t)j;
    }
    tp.top = tp.cur;
}

static void hist_compact(void)        /* more than HIST_LEVELS: the oldest becomes permanent */
{
    uint32_t k, i, j;
    while ((uint32_t)(tp.top - tp.base) > HIST_LEVELS) tp.base++;
    for (k = 0; k < NTRACK; k++) {
        ttrack_t *t = &tt[k];
        for (i = j = 0; i < t->n; i++) {
            tev_t e = t->ev[i];
            if (e.del && e.del <= tp.base) continue;          /* deleted for good */
            t->ev[j++] = e;
        }
        t->n = (uint16_t)j;
    }
    if (tp.top > 60000u) {                                     /* renumber before the serials run out */
        uint16_t d = tp.base;
        for (k = 0; k < NTRACK; k++)
            for (i = 0; i < tt[k].n; i++) {
                tev_t *e = &tt[k].ev[i];
                e->add = e->add > d ? (uint16_t)(e->add - d) : 0u;
                if (e->del) e->del = (uint16_t)(e->del - d);
            }
        {                                                      /* keep each record at its new serial's slot */
            hop_t h[HIST_LEVELS + 1u];
            uint32_t s;
            for (s = 0; s <= HIST_LEVELS; s++) h[s] = tp.hop[s];
            for (s = tp.base + 1u; s <= tp.top; s++) tp.hop[(s - d) % (HIST_LEVELS + 1u)] = h[s % (HIST_LEVELS + 1u)];
        }
        tp.top = (uint16_t)(tp.top - d);
        tp.cur = (uint16_t)(tp.cur - d);
        tp.base = 0;
    }
    tape_recursor();
}

static uint16_t hist_new(uint32_t type, uint32_t track)
{
    hop_t *h;
    if (tp.cur != tp.top) hist_drop_future();
    tp.top++;
    tp.cur = tp.top;
    h = &tp.hop[tp.top % (HIST_LEVELS + 1u)];
    h->type = (uint8_t)type;
    h->track = (uint8_t)track;
    h->nev = 0;
    hist_compact();
    return tp.top;
}

static int tape_undo(void)
{
    if (tp.take) tp.take = 0;                  /* an undo ends the take being recorded */
    if (tp.cur <= tp.base) return 0;
    tp.cur--;
    tape_all_off();
    return 1;
}

static int tape_redo(void)
{
    if (tp.cur >= tp.top) return 0;
    tp.cur++;
    tape_all_off();
    return 1;
}

/* ---- recording */
static void tape_insert(uint32_t k, uint32_t tick, uint32_t dur, uint32_t note, uint32_t vel)
{
    ttrack_t *t = &tt[k];
    uint32_t at, i;
    if (t->n >= TAPE_MAXEV) { tp.events_dropped++; return; }
    if (!tp.take) tp.take = hist_new(OP_TAKE, k);
    at = lower_bound(t, tick + 1u);            /* after the events of the same tick: stable order */
    for (i = t->n; i > at; i--) t->ev[i] = t->ev[i - 1u];
    t->ev[at].tick = tick;
    t->ev[at].dur = (uint16_t)(dur < 1u ? 1u : dur > 65535u ? 65535u : dur);
    t->ev[at].note = (uint8_t)note;
    t->ev[at].vel = (uint8_t)vel;
    t->ev[at].add = tp.take;
    t->ev[at].del = 0;
    t->n++;
    tp.hop[tp.take % (HIST_LEVELS + 1u)].nev++;
    t->cur = lower_bound(t, tp.pos);
}

static int tape_recording(void) { return tp.armed && tp.playing; }

static void tape_rec_start_note(uint32_t note, uint32_t drum)
{
    uint32_t i;
    if (!tape_recording()) return;
    for (i = 0; i < 16u; i++)
        if (!tp.rec[i].on) {
            tp.rec[i].on = 1;
            tp.rec[i].note = (uint8_t)note;
            tp.rec[i].drum = (uint8_t)drum;
            tp.rec[i].start = tp.pos;
            return;
        }
}

static void tape_rec_close(uint32_t i)
{
    uint32_t s = tp.rec[i].start, d;
    tp.rec[i].on = 0;
    if (tp.pos >= s) d = tp.pos - s;
    else d = tp.loop_end - s + tp.pos;         /* held across the loop's end */
    if (tp.rec[i].drum) d = 1;
    tape_insert(tp.track, s, d, tp.rec[i].note, tp.rec[i].drum ? (100u | TEV_DRUM) : 100u);
}

static void tape_rec_end_note(uint32_t note, uint32_t drum)
{
    uint32_t i;
    for (i = 0; i < 16u; i++)
        if (tp.rec[i].on && tp.rec[i].note == note && tp.rec[i].drum == drum) {
            tape_rec_close(i);
            return;
        }
}

static void tape_take_end(void)               /* close what is held, end the take */
{
    uint32_t i;
    for (i = 0; i < 16u; i++)
        if (tp.rec[i].on) tape_rec_close(i);
    tp.take = 0;
}

static void tape_erase_track(uint32_t k)
{
    ttrack_t *t = &tt[k];
    uint32_t i, any = 0;
    uint16_t s;
    for (i = 0; i < t->n; i++) any |= (uint32_t)tev_visible(&t->ev[i]);
    if (!any) return;
    tape_take_end();
    s = hist_new(OP_ERASE, k);
    for (i = 0; i < t->n; i++)
        if (tev_visible(&t->ev[i])) { t->ev[i].del = s; tp.hop[s % (HIST_LEVELS + 1u)].nev++; }
    tape_all_off();
}

/* ---- transport */
static void tape_play(int on)
{
    if (on && !tp.playing) {
        tp.playing = 1;
        tape_recursor();
        if (tp.armed) trk_snd[tp.track] = live_snd;
    } else if (!on && tp.playing) {
        tape_take_end();
        tp.playing = 0;
        tape_all_off();
    }
}

static void tape_arm(int on)
{
    if (on && !tp.armed) {
        tp.armed = 1;
        if (!tp.playing) tape_play(1);       /* REC from stop: record from here */
        trk_snd[tp.track] = live_snd;        /* the track plays the sound it is recorded with */
    } else if (!on && tp.armed) {
        tape_take_end();
        tp.armed = 0;
    }
}

static void tape_seek(uint32_t tick)
{
    tape_take_end();
    tape_all_off();
    tp.pos = tp.loop && tick >= tp.loop_end ? 0 : tick;
    tape_recursor();
}

static void tape_set_track(uint32_t k)
{
    if (k >= NTRACK || k == tp.track) return;
    tape_take_end();
    tp.track = (uint8_t)k;
}

static void tape_set_loop(uint32_t bars, uint32_t bpm, uint32_t loop)
{
    tp.bars = (uint8_t)bars;
    tp.bpm = (uint8_t)bpm;
    tp.loop = (uint8_t)loop;
    tp.loop_end = loop_ticks();
    if (tp.loop && tp.pos >= tp.loop_end) tape_seek(0);
}

/* one tick: end the tape notes due, start the events of this tick, move on */
static void tape_tick(void)
{
    uint32_t k, i;
    if (!tp.playing) return;
    for (k = 0; k < NTRACK; k++) {
        ttrack_t *t = &tt[k];
        for (i = 0; i < 16u; i++)
            if (tp.play[k][i].on && tp.play[k][i].off == tp.pos) {
                tp.play[k][i].on = 0;
                if (!tp.play[k][i].drum) synth_note_off(k, tp.play[k][i].note, SRC_TAPE);
            }
        while (t->cur < t->n && t->ev[t->cur].tick == tp.pos) {
            const tev_t *e = &t->ev[t->cur++];
            if (tev_visible(e)) {
                uint32_t off = e->tick + e->dur;
                if (tp.loop && off >= tp.loop_end) off -= tp.loop_end;
                tape_voice_on(k, e->note, (e->vel & TEV_DRUM) != 0, e->vel & 0x7Fu, off);
            }
        }
    }
    tp.pos++;
    if (tp.loop && tp.pos >= tp.loop_end) {
        tp.pos = 0;
        tape_recursor();
    } else if (tp.pos >= TAPE_MAX_TICKS) {
        tape_play(0);
    }
}
