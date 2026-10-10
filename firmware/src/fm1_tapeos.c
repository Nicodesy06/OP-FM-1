/* SPDX-License-Identifier: GPL-3.0-only */
/* FM1 Tape OS on the M-VAVE FM1: Dinghy's platform (platform.c and below, unchanged: boot, boot guard,
 * USB rescue, updater, USB-MIDI, USB audio, panel scan, LCD, LEDs) + the shared application (src/tapeos.c).
 * This file is the adapter between them: the panel's scanned state -> tos_panel(), the audio interrupt ->
 * tos_audio(), the frame -> the LCD (only the rows that changed), the lights.
 * Build: firmware/tools/build.py (JieLi pi32v2 clang). Not flashed by anything in this repository. */
#define APP_NAME "TAPE OS"
#include "platform.c"

#define TOS_POOL __attribute__((section(".pool")))   /* app.ld: the 336 KB pool, zeroed at boot */
#define TOS_FRAME_MS 40u                              /* 25 frames a second on the device */
#include "tapeos.c"

/* the audio interrupt: the application, timed (the MIXER screen shows the load: the share of the 5.8 ms
 * between two interrupts spent here, the highest of the last ~0.5 s) */
static uint32_t cpu_peak, cpu_n;
static void app_audio(int32_t *out, uint32_t n)
{
    uint32_t t0 = fm1_ticks(), dt, pm;
    tos_audio(out, n);
    dt = fm1_ticks() - t0;
    pm = dt / ((n * FM1_TICKS_PER_US * 1000u) / 44100u + 1u);   /* the period in ticks / 1000 (256 frames: 139) */
    if (pm > cpu_peak) cpu_peak = pm;
    if (++cpu_n >= 86u) { tos_cpu(cpu_peak ? cpu_peak : 1u); cpu_peak = 0; cpu_n = 0; }
}

/* ---- the screen: send the rows that changed since the last frame, 8 at a time, bytes swapped for the
 * ST7789 (two line buffers: one fills while the other is sent) */
#define BAND 8u
static uint32_t row_sum[TOS_H];
static uint16_t band_buf[2][TOS_W * BAND];

static uint32_t row_hash(const uint16_t *p)
{
    uint32_t h = 2166136261u, i;
    for (i = 0; i < TOS_W; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}

static void push_screen(void)
{
    const uint16_t *f = tos_fb();
    uint32_t y, nb = 0;
    for (y = 0; y < TOS_H; y += BAND) {
        uint32_t r, changed = 0;
        for (r = 0; r < BAND; r++) {
            uint32_t h = row_hash(f + (y + r) * TOS_W);
            if (h != row_sum[y + r]) { row_sum[y + r] = h; changed = 1; }
        }
        if (changed) {
            uint16_t *b = band_buf[nb];
            uint32_t i;
            lcd_sync();                                /* the buffer about to be refilled may still be sending */
            for (i = 0; i < TOS_W * BAND; i++) {
                uint16_t c = f[y * TOS_W + i];
                b[i] = (uint16_t)((c >> 8) | (c << 8));
            }
            lcd_blit(0, y, TOS_W, BAND, b);
            nb ^= 1u;
        }
    }
}

static void app_main(void)
{
    uint32_t prev_keys = 0, last_serial = 0, k;
    lcd_fill(0, 0, 240, 240, C_BG);
    tos_init();
    for (;;) {
        uint32_t rel, pressed, keys, down, up, b;
        uint8_t st, d1, d2;
        platform_poll();
        pressed = fm1_input_edges(&rel);            /* buttons: presses and releases since the last pass */
        for (b = 0; b < TOS_NBTN; b++) {
            if (pressed >> b & 1u) tos_panel(TOS_PE_BTN_DOWN, b, 0);
            if (rel >> b & 1u) tos_panel(TOS_PE_BTN_UP, b, 0);
        }
        keys = fm1_in.notes;                          /* keys: a press shorter than a pass still counts */
        down = (keys & ~prev_keys) | fm1_input_note_edges();
        up = prev_keys & ~keys;
        for (k = 0; k < TOS_NKEYS; k++) {
            if (down >> k & 1u) tos_panel(TOS_PE_KEY_DOWN, k, 0);
            if ((up >> k & 1u) || ((down >> k & 1u) && !(keys >> k & 1u))) tos_panel(TOS_PE_KEY_UP, k, 0);
        }
        prev_keys = keys;
        for (k = 0; k < TOS_NENC; k++) {
            int32_t d = fm1_enc_take(k);
            if (d) tos_panel(TOS_PE_ENC, k, d);
        }
        while (midi_get(&st, &d1, &d2)) tos_midi(st, d1, d2);
        tos_ui(fm1_ms);
        if (tos_fb_serial() != last_serial) {
            last_serial = tos_fb_serial();
            push_screen();
            {
                uint32_t lk = tos_leds_keys(), lb = tos_leds_buttons();
                for (k = 0; k < TOS_NKEYS; k++) fm1_led_key(KEY_LED(k), lk >> k & 1u);
                for (k = 0; k < TOS_NBTN; k++) fm1_led_key(k, lb >> k & 1u);
            }
        }
    }
}
