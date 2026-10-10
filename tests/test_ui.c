/* SPDX-License-Identifier: GPL-3.0-only */
/* The screens, from the real renderer: every screen in a few states written as PPM (build/test/ui_*.ppm,
 * 240 x 240), texts that fit their places, pictures that follow the instrument (a knob, a note, the tape). */
#include "tapeos.c"
#include "../platform/host/host.h"

static uint32_t region_hash(int32_t x, int32_t y, int32_t w, int32_t h)
{
    uint32_t hsh = 2166136261u;
    int32_t i, j;
    for (j = y; j < y + h; j++)
        for (i = x; i < x + w; i++) { hsh ^= fb[j * TOS_W + i]; hsh *= 16777619u; }
    return hsh;
}

static void shot(const char *name)
{
    char p[96];
    host_run(40);                                  /* at least one new frame */
    snprintf(p, sizeof p, "build/test/ui_%s.ppm", name);
    host_write_ppm(p);
}

static void test_texts_fit(void)
{
    uint32_t i, k;
    for (i = 0; i < NPARAM; i++) {
        CHECK(txt_w(PD[i].name, 1) <= 58u, "label %s fits a dial (58 px)", PD[i].name);
        if (PD[i].names)
            for (k = 0; k <= (uint32_t)(PD[i].max - PD[i].min); k++)
                CHECK(txt_w(PD[i].names[k], 1) <= 58u, "value %s fits a dial", PD[i].names[k]);
    }
    for (i = 0; i < NSCR; i++)
        CHECK(6u + txt_w(SCR_NAME[i], 1) + 8u + txt_w("ANALOG", 1) < 148u, "title %s clears BAR", SCR_NAME[i]);
    for (i = 0; i < NDRUM; i++) CHECK(txt_w(DRUM_NAME[i], 1) <= 49u, "drum name %s fits its pad", DRUM_NAME[i]);
    CHECK(txt_w("NO UNDO", 2) <= 156u && txt_w("UNDO", 3) <= 156u, "popup texts fit");
}

static void test_screens(void)
{
    uint32_t a, b;
    host_frames = 0; tos_init();
    host_run(50);
    shot("synth_idle");
    a = region_hash(8, 24, 108, 76);
    host_enc(TOS_ENC_ALGORITHM, 1);                 /* WAVE: SAW -> SQUARE */
    host_run(40);
    b = region_hash(8, 24, 108, 76);
    CHECK(a != b, "the waveform picture follows WAVE");
    a = region_hash(124, 24, 108, 76);
    host_enc(TOS_ENC_KNOB1, -40);                    /* CUTOFF */
    host_run(40);
    CHECK(a != region_hash(124, 24, 108, 76), "the filter picture follows CUTOFF");
    a = region_hash(8, 104, 224, 60);
    tos_panel(TOS_PE_KEY_DOWN, 12, 0);
    tos_panel(TOS_PE_KEY_DOWN, 16, 0);
    host_run(120);
    CHECK(a != region_hash(8, 104, 224, 60), "the scope and the voices follow the notes");
    shot("synth_playing");
    tos_panel(TOS_PE_KEY_UP, 12, 0);
    tos_panel(TOS_PE_KEY_UP, 16, 0);
    host_btn(TOS_BTN_ENV); shot("env");
    host_btn(TOS_BTN_LFO); host_enc(TOS_ENC_KNOB2, 60); shot("lfo");
    host_btn(TOS_BTN_FX); host_enc(TOS_ENC_KNOB3, 70); shot("fx");
    host_btn(TOS_BTN_SEL);
    a = region_hash(8, 26, 224, 130);
    tos_panel(TOS_PE_KEY_DOWN, 0, 0); host_run(30);
    CHECK(a != region_hash(8, 26, 224, 130), "a drum pad lights with its voice");
    shot("drum");
    tos_panel(TOS_PE_KEY_UP, 0, 0);
    host_btn(TOS_BTN_HOME);
    host_btn(TOS_BTN_SEQ);
    host_btn(TOS_BTN_REC);
    host_run(200); host_key(12, 150); host_run(100); host_key(16, 300); host_key(19, 100);
    host_btn(TOS_BTN_REC);
    host_run(300);
    shot("tape");
    a = region_hash(8, 78, 224, 84);
    host_combo(TOS_BTN_SEQ, TOS_BTN_OCT_DOWN);
    host_run(40);
    shot("tape_undo_popup");
    host_run(1000);
    CHECK(a != region_hash(8, 78, 224, 84), "the tracks show the undo");
    host_btn(TOS_BTN_GLO);
    tos_panel(TOS_PE_KEY_DOWN, 12, 0); host_run(100);
    shot("mixer");
    tos_panel(TOS_PE_KEY_UP, 12, 0);
}

int main(void)
{
    test_texts_fit();
    test_screens();
    CHECK(sizeof fb == 240u * 240u * 2u, "frame 240 x 240 RGB565");
    printf("test_ui: %d passed, %d failed\n", t_pass, t_fail);
    return t_fail != 0;
}
