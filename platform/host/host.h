/* SPDX-License-Identifier: GPL-3.0-only */
/* The FM1 on a computer, for the tests: the application (src/tapeos.c, included before this file) runs as on
 * the device, with the device's clock driven by the audio rendered: 256-frame audio blocks (Dinghy's audio
 * interrupt: 256 frames at 44.1 kHz), the main loop every millisecond of that clock. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HOST_BLOCK 256u
static uint64_t host_frames;          /* device time, in frames */
static int16_t *host_wav;              /* captured audio (stereo), when recording */
static uint32_t host_wav_n, host_wav_cap;

static uint32_t host_ms(void) { return (uint32_t)(host_frames * 1000u / TOS_FS); }

static void host_capture(uint32_t frames)
{
    free(host_wav);
    host_wav = calloc(frames * 2u, sizeof(int16_t));
    host_wav_cap = frames;
    host_wav_n = 0;
}

/* run the device for ms milliseconds */
static void host_run(uint32_t ms)
{
    uint64_t end = host_frames + (uint64_t)ms * TOS_FS / 1000u;
    while (host_frames < end) {
        int32_t buf[2u * HOST_BLOCK];
        uint32_t i, ms0 = host_ms();
        memset(buf, 0, sizeof buf);
        tos_audio(buf, HOST_BLOCK);
        for (i = 0; i < HOST_BLOCK && host_wav && host_wav_n < host_wav_cap; i++, host_wav_n++) {
            int32_t l = buf[2u * i], r = buf[2u * i + 1u];
            host_wav[2u * host_wav_n] = (int16_t)(l > 32767 ? 32767 : l < -32767 ? -32767 : l);
            host_wav[2u * host_wav_n + 1u] = (int16_t)(r > 32767 ? 32767 : r < -32767 ? -32767 : r);
        }
        host_frames += HOST_BLOCK;
        for (i = ms0 + 1u; i <= host_ms(); i++) tos_ui(i);
    }
}

static void host_btn(uint32_t b) { tos_panel(TOS_PE_BTN_DOWN, b, 0); host_run(2); tos_panel(TOS_PE_BTN_UP, b, 0); host_run(2); }
static void host_combo(uint32_t hold, uint32_t b)
{
    tos_panel(TOS_PE_BTN_DOWN, hold, 0);
    host_run(2);
    host_btn(b);
    tos_panel(TOS_PE_BTN_UP, hold, 0);
    host_run(2);
}
static void host_key(uint32_t k, uint32_t ms) { tos_panel(TOS_PE_KEY_DOWN, k, 0); host_run(ms); tos_panel(TOS_PE_KEY_UP, k, 0); }
static void host_enc(uint32_t e, int32_t d) { tos_panel(TOS_PE_ENC, e, d); host_run(2); }

static void host_write_wav(const char *path)
{
    FILE *f = fopen(path, "wb");
    uint32_t n = host_wav_n, bytes = n * 4u, v;
    if (!f) return;
    fwrite("RIFF", 1, 4, f); v = 36u + bytes; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    v = 16; fwrite(&v, 4, 1, f); v = 0x00020001u; fwrite(&v, 4, 1, f); v = TOS_FS; fwrite(&v, 4, 1, f);
    v = TOS_FS * 4u; fwrite(&v, 4, 1, f); v = 0x00100004u; fwrite(&v, 4, 1, f);
    fwrite("data", 1, 4, f); fwrite(&bytes, 4, 1, f); fwrite(host_wav, 4, n, f);
    fclose(f);
}

static void host_write_ppm(const char *path)
{
    FILE *f = fopen(path, "wb");
    uint32_t i;
    const uint16_t *p = tos_fb();
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", TOS_W, TOS_H);
    for (i = 0; i < (uint32_t)(TOS_W * TOS_H); i++) {
        uint8_t px[3] = {(uint8_t)((p[i] >> 11) << 3), (uint8_t)(((p[i] >> 5) & 63u) << 2), (uint8_t)((p[i] & 31u) << 3)};
        fwrite(px, 1, 3, f);
    }
    fclose(f);
}

static uint32_t fnv1a(const void *d, uint32_t n)
{
    const uint8_t *p = d;
    uint32_t h = 2166136261u;
    while (n--) { h ^= *p++; h *= 16777619u; }
    return h;
}

/* ---- a tiny test framework */
static int t_fail, t_pass;
#define CHECK(c, ...) do { if (c) t_pass++; else { t_fail++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
