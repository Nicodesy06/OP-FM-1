/* SPDX-License-Identifier: GPL-3.0-only */
/* The sound: a note sounds and stops, the knobs change the DSP, the drums, the meters; WAV renders whose
 * hashes are compared with tests/golden_audio.txt (GOLDEN_UPDATE=1 rewrites it: say why in the commit);
 * the cost of the audio code on this computer. */
#include <math.h>
#include <time.h>
#include "tapeos.c"
#include "../platform/host/host.h"

static double rms(uint32_t from, uint32_t to)
{
    double s = 0;
    uint32_t i;
    for (i = from; i < to && i < host_wav_n; i++) s += (double)host_wav[2u * i] * host_wav[2u * i];
    return to > from ? sqrt(s / (to - from)) : 0;
}

static double hf_rms(uint32_t from, uint32_t to)   /* energy of the first difference: the high frequencies */
{
    double s = 0;
    uint32_t i;
    for (i = from + 1u; i < to && i < host_wav_n; i++) {
        double d = (double)host_wav[2u * i] - host_wav[2u * (i - 1u)];
        s += d * d;
    }
    return to > from ? sqrt(s / (to - from)) : 0;
}

static FILE *golden_out;
static char golden_in[4096];

static void golden(const char *name)
{
    char path[128], line[96];
    uint32_t h = fnv1a(host_wav, host_wav_n * 4u);
    snprintf(path, sizeof path, "build/test/%s.wav", name);
    host_write_wav(path);
    snprintf(line, sizeof line, "%s %08x\n", name, h);
    if (golden_out) { fputs(line, golden_out); return; }
    CHECK(strstr(golden_in, line) != 0, "%s: hash %08x not in tests/golden_audio.txt (render changed?)", name, h);
}

static void test_note(void)
{
    double on, off;
    host_frames = 0; tos_init();
    host_capture(TOS_FS * 2u);
    host_run(100);
    host_key(12, 800);                              /* F4, 800 ms */
    host_run(1100);
    on = rms(TOS_FS * 3u / 10u, TOS_FS * 8u / 10u);
    off = rms(TOS_FS * 19u / 10u, TOS_FS * 2u);
    CHECK(on > 1500.0, "a key sounds (rms %.0f)", on);
    CHECK(off < 20.0, "and stops after its release (rms %.0f)", off);
    golden("note_saw_lead");
}

static void test_cutoff(void)
{
    double lo, hi;
    host_frames = 0; tos_init();
    host_capture(TOS_FS);
    host_enc(TOS_ENC_KNOB1, -70);                   /* CUTOFF down */
    tos_panel(TOS_PE_KEY_DOWN, 12, 0); host_run(400);
    lo = hf_rms(TOS_FS / 5u, TOS_FS * 2u / 5u);
    host_enc(TOS_ENC_KNOB1, 70);                    /* back up */
    host_run(400);
    hi = hf_rms(TOS_FS * 3u / 5u, TOS_FS * 4u / 5u);
    tos_panel(TOS_PE_KEY_UP, 12, 0);
    CHECK(hi > lo * 2.0, "knob 1 (CUTOFF) opens the filter (hf %.0f -> %.0f)", lo, hi);
}

static void test_waves(void)
{
    uint32_t w;
    host_frames = 0; tos_init();
    for (w = 0; w < 7u; w++) {
        char n[32];
        host_capture(TOS_FS / 2u);
        tos_panel(TOS_PE_KEY_DOWN, 12, 0); host_run(300); tos_panel(TOS_PE_KEY_UP, 12, 0); host_run(200);
        CHECK(rms(0, TOS_FS / 4u) > 500.0, "wave %u sounds", w);
        snprintf(n, sizeof n, "wave_%s", N_WAVE[w]);
        golden(n);
        host_enc(TOS_ENC_ALGORITHM, 1);
    }
}

static void test_drums(void)
{
    uint32_t k;
    host_frames = 0; tos_init();
    host_btn(TOS_BTN_SEL);
    host_capture(TOS_FS * 4u);
    for (k = 0; k < 8u; k++) {
        static const uint8_t WK[8] = {0, 2, 4, 5, 7, 9, 11, 12};   /* the white keys F3 .. C5 */
        uint32_t f0 = host_wav_n;
        host_key(WK[k], 40);
        host_run(400);
        CHECK(rms(f0, f0 + TOS_FS / 10u) > 300.0, "drum %s sounds (rms %.0f)", DRUM_NAME[k], rms(f0, f0 + TOS_FS / 10u));
    }
    golden("drums_8");
}

static void test_mixer(void)
{
    double a, b;
    host_frames = 0; tos_init();
    host_capture(TOS_FS);
    tos_panel(TOS_PE_KEY_DOWN, 12, 0); host_run(300);
    a = rms(TOS_FS / 10u, TOS_FS / 4u);
    CHECK(mx_peak[0] > 1000, "the channel 1 meter reads the signal (%d)", mx_peak[0]);
    host_btn(TOS_BTN_GLO);
    tos_panel(TOS_PE_BTN_DOWN, TOS_BTN_GLO, 0); host_run(2);
    host_key(0, 10);                                 /* GLO + F3: mute 1 */
    tos_panel(TOS_PE_BTN_UP, TOS_BTN_GLO, 0);
    host_run(300);
    b = rms(TOS_FS / 2u + 2000u, TOS_FS * 6u / 10u);
    tos_panel(TOS_PE_KEY_UP, 12, 0);
    CHECK(mx_mute[0] == 1, "channel 1 muted");
    CHECK(a > 1000.0 && b < 5.0, "mute silences it (%.0f -> %.0f)", a, b);
}

static void test_cpu(void)
{
    /* 8 synth voices + 8 drums + the delay: the worst case, timed on this computer (not the FM1) */
    uint32_t k, n = TOS_FS * 10u;
    clock_t t0;
    double s;
    int32_t buf[2u * 256u];
    host_frames = 0; tos_init();
    cmd_push(CMD_PARAM, PR_FXMIX, 80);
    for (k = 0; k < 8u; k++) cmd_push(CMD_NOTE_ON, 48u + k * 3u, 100);
    t0 = clock();
    for (k = 0; k < n; k += 256u) {
        memset(buf, 0, sizeof buf);
        if (k % 4410u < 256u) { uint32_t d; for (d = 0; d < 8u; d++) drum_hit(d, 0, 100); }
        tos_audio(buf, 256u);
    }
    s = (double)(clock() - t0) / CLOCKS_PER_SEC;
    printf("cpu (host): 10 s of audio, 8 voices + 8 drums + delay: %.3f s = %.1f %% of real time\n", s, s * 10.0);
    CHECK(s < 10.0, "faster than real time on the host");
}

int main(void)
{
    FILE *g = fopen("tests/golden_audio.txt", "r");
    if (getenv("GOLDEN_UPDATE")) golden_out = fopen("tests/golden_audio.txt", "w");
    else if (g) { size_t n = fread(golden_in, 1, sizeof golden_in - 1u, g); golden_in[n] = 0; }
    if (g) fclose(g);
    test_note();
    test_cutoff();
    test_waves();
    test_drums();
    test_mixer();
    test_cpu();
    if (golden_out) fclose(golden_out);
    printf("test_audio: %d passed, %d failed\n", t_pass, t_fail);
    return t_fail != 0;
}
