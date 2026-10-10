/* SPDX-License-Identifier: GPL-3.0-only */
/* The tape: record, play, overdub, loop recording, undo / redo, erase, timing. Driven through the panel
 * (the same path as the FM1's buttons and keys), checked on the tape's state. */
#include "tapeos.c"
#include "../platform/host/host.h"

static uint32_t visible(uint32_t k)
{
    uint32_t i, n = 0;
    for (i = 0; i < tt[k].n; i++) n += (uint32_t)tev_visible(&tt[k].ev[i]);
    return n;
}

static uint32_t tape_voices(void)
{
    uint32_t i, n = 0;
    for (i = 0; i < NSVOICE; i++) n += sv[i].stage && sv[i].stage != 3 && sv[i].src == SRC_TAPE;
    return n;
}

static void reset(void) { host_frames = 0; tos_init(); host_run(10); }

static void test_record_play(void)
{
    uint32_t t0;
    reset();
    host_btn(TOS_BTN_SEQ);                         /* TAPE */
    CHECK(ui.scr == SCR_TAPE, "SEQ shows TAPE");
    host_btn(TOS_BTN_REC);                         /* from stop: arm + play */
    CHECK(tp.armed && tp.playing, "REC from stop arms and plays");
    host_run(100);
    t0 = tp.pos;
    host_key(0, 200);                              /* F3 for 200 ms */
    host_run(50);
    CHECK(visible(0) == 1, "one note recorded (got %u)", visible(0));
    CHECK(tt[0].ev[0].note == 53, "note F3 = 53 (got %u)", tt[0].ev[0].note);
    /* the command reaches the audio side at the next block (<= 256 frames = 8 ticks after the key) */
    CHECK(tt[0].ev[0].tick >= t0 && tt[0].ev[0].tick <= t0 + 8u, "start tick %u near %u", tt[0].ev[0].tick, t0);
    CHECK(tt[0].ev[0].dur >= 270u && tt[0].ev[0].dur <= 285u, "200 ms = 275.6 ticks (got %u)", tt[0].ev[0].dur);
    CHECK(tp.top == 1 && tp.cur == 1, "one take in the history");
    host_btn(TOS_BTN_PLAY);                        /* stop */
    CHECK(!tp.playing, "PLAY while playing stops");
    host_btn(TOS_BTN_REC);                         /* disarm (from stop: REC arms and plays, so twice) */
    host_btn(TOS_BTN_PLAY);
    if (tp.armed) host_btn(TOS_BTN_REC);
    host_combo(TOS_BTN_SEQ, TOS_BTN_PLAY);         /* back to the start */
    CHECK(tp.pos < 64u, "SEQ + PLAY rewinds (pos %u: the combo itself takes ~10 ms)", tp.pos);
    if (!tp.playing) host_btn(TOS_BTN_PLAY);
    {   /* it plays back: a tape voice sounds while the event plays */
        uint32_t i, heard = 0;
        for (i = 0; i < 400u && !heard; i++) { host_run(5); heard = tape_voices() > 0; }
        CHECK(heard, "the recorded note plays back");
    }
}

static void test_overdub_undo_redo(void)
{
    reset();
    host_btn(TOS_BTN_SEQ);
    host_btn(TOS_BTN_REC);
    host_run(50); host_key(0, 100); host_run(50);
    host_btn(TOS_BTN_REC);                         /* end take 1, still playing */
    CHECK(tp.playing && !tp.armed, "REC while recording: stop recording, keep playing");
    host_btn(TOS_BTN_REC);                         /* take 2: overdub */
    host_run(30); host_key(4, 100); host_key(7, 100); host_run(30);
    host_btn(TOS_BTN_REC);
    CHECK(visible(0) == 3, "3 notes after the overdub (got %u)", visible(0));
    CHECK(tp.top == 2, "two takes (top %u)", tp.top);
    host_combo(TOS_BTN_SEQ, TOS_BTN_OCT_DOWN);     /* UNDO */
    CHECK(visible(0) == 1, "undo removes the overdub exactly (got %u)", visible(0));
    CHECK(ui.popup == 1, "UNDO popup (got %u)", ui.popup);
    CHECK(ui.oct == 0, "SEQ + OCT- does not change the octave");
    host_combo(TOS_BTN_SEQ, TOS_BTN_OCT_UP);       /* REDO */
    CHECK(visible(0) == 3, "redo brings it back (got %u)", visible(0));
    CHECK(ui.popup == 2, "REDO popup");
    host_combo(TOS_BTN_SEQ, TOS_BTN_OCT_UP);
    CHECK(ui.popup == 4, "nothing to redo: NO REDO");
    /* a new take after an undo drops the redo */
    host_combo(TOS_BTN_SEQ, TOS_BTN_OCT_DOWN);
    CHECK(tp.cur == 1 && tp.top == 2, "undone: cur 1 top 2");
    host_btn(TOS_BTN_REC); host_run(20); host_key(2, 60); host_run(20); host_btn(TOS_BTN_REC);
    CHECK(tp.top == 2 && tp.cur == 2, "the new take replaces the undone one (top %u cur %u)", tp.top, tp.cur);
    CHECK(visible(0) == 2, "first take + new take (got %u)", visible(0));
    host_combo(TOS_BTN_SEQ, TOS_BTN_OCT_UP);
    CHECK(ui.popup == 4 && visible(0) == 2, "the dropped take cannot come back");
    /* undo while stopped too */
    host_btn(TOS_BTN_PLAY);
    CHECK(!tp.playing, "stopped");
    host_combo(TOS_BTN_SEQ, TOS_BTN_OCT_DOWN);
    CHECK(visible(0) == 1, "undo while stopped (got %u)", visible(0));
}

static void test_seven_levels(void)
{
    uint32_t i;
    reset();
    host_btn(TOS_BTN_SEQ);
    host_btn(TOS_BTN_REC);
    host_btn(TOS_BTN_REC);                         /* playing, not recording */
    for (i = 0; i < 9u; i++) {
        host_btn(TOS_BTN_REC);
        host_run(10); host_key(i, 30); host_run(10);
        host_btn(TOS_BTN_REC);
    }
    CHECK(visible(0) == 9, "9 takes recorded (got %u)", visible(0));
    for (i = 0; i < 7u; i++) host_combo(TOS_BTN_SEQ, TOS_BTN_OCT_DOWN);
    CHECK(visible(0) == 2, "7 undos leave the 2 oldest (got %u)", visible(0));
    host_combo(TOS_BTN_SEQ, TOS_BTN_OCT_DOWN);
    CHECK(ui.popup == 3 && visible(0) == 2, "the 8th undo: NO UNDO (popup %u, %u visible)", ui.popup, visible(0));
    for (i = 0; i < 7u; i++) host_combo(TOS_BTN_SEQ, TOS_BTN_OCT_UP);
    CHECK(visible(0) == 9, "7 redos bring all back (got %u)", visible(0));
    CHECK(tt[0].n == 9, "storage: 9 events, no copies (n %u)", tt[0].n);
}

static void test_loop_recording(void)
{
    uint32_t len, i;
    reset();
    host_btn(TOS_BTN_SEQ);
    host_enc(TOS_ENC_KNOB2, -3);                   /* BARS 4 -> 1: a 2 s loop at 120 */
    CHECK(tp.bars == 1, "1 bar (got %u)", tp.bars);
    len = tp.loop_end;
    CHECK(len == 2756u, "1 bar at 120 = 2756 ticks (got %u)", len);
    host_btn(TOS_BTN_REC);
    host_run(1800);                                /* near the end of the loop */
    host_key(5, 400);                              /* held across the loop's end */
    host_run(200);
    CHECK(tp.pos < 1000u, "the loop wrapped (pos %u)", tp.pos);
    CHECK(visible(0) == 1, "one note (got %u)", visible(0));
    CHECK(tt[0].ev[0].tick > 2400u && tt[0].ev[0].dur >= 540u && tt[0].ev[0].dur <= 560u,
          "start %u, dur %u (400 ms = 551 ticks) across the end", tt[0].ev[0].tick, tt[0].ev[0].dur);
    for (i = 0; i < 3u; i++) { host_run(200); host_key(10 + i, 80); }    /* more layers, same take */
    host_btn(TOS_BTN_REC);
    CHECK(tp.top == 1, "a loop recording is one take (top %u)", tp.top);
    host_combo(TOS_BTN_SEQ, TOS_BTN_OCT_DOWN);
    CHECK(visible(0) == 0, "and undoes in one step (got %u)", visible(0));
}

static void test_erase_and_drums(void)
{
    reset();
    host_btn(TOS_BTN_SEL);                         /* DRUM */
    CHECK(ui.instr_drum, "SEL: drums on the keys");
    host_btn(TOS_BTN_REC);
    host_run(20); host_key(0, 30); host_key(2, 30); host_run(20);
    host_btn(TOS_BTN_REC);
    CHECK(visible(0) == 2 && (tt[0].ev[0].vel & TEV_DRUM), "drum hits recorded as drum events");
    host_combo(TOS_BTN_SAVE, TOS_BTN_REC);         /* erase track 1 */
    CHECK(visible(0) == 0 && tp.top == 2, "erase is an operation (top %u)", tp.top);
    host_combo(TOS_BTN_SEQ, TOS_BTN_OCT_DOWN);
    CHECK(visible(0) == 2, "and undoes");
}

static void test_tracks(void)
{
    reset();
    host_btn(TOS_BTN_SEQ);
    host_enc(TOS_ENC_KNOB1, 1);                    /* TRACK 2 */
    CHECK(tp.track == 1, "knob 1 picks track 2");
    host_btn(TOS_BTN_REC);
    host_run(20); host_key(3, 50); host_run(20);
    host_btn(TOS_BTN_REC);
    CHECK(visible(1) == 1 && visible(0) == 0, "recorded on track 2 only");
    host_btn(TOS_BTN_PLAY);
    CHECK(tp.armed == 0 && tp.playing == 0, "stopped");
}

static void test_clock(void)
{
    reset();
    host_btn(TOS_BTN_SEQ);
    host_btn(TOS_BTN_PLAY);
    {
        uint32_t p0 = tp.pos;
        uint64_t f0 = host_frames;
        host_run(1000);
        /* the tape moves exactly one tick per 32 frames of audio */
        CHECK(tp.pos - p0 == (uint32_t)((host_frames - f0) / CTL), "tape clock = audio clock (%u ticks)", tp.pos - p0);
    }
}

int main(void)
{
    test_record_play();
    test_overdub_undo_redo();
    test_seven_levels();
    test_loop_recording();
    test_erase_and_drums();
    test_tracks();
    test_clock();
    printf("test_tape: %d passed, %d failed\n", t_pass, t_fail);
    return t_fail != 0;
}
