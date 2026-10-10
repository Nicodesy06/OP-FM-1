/* SPDX-License-Identifier: GPL-3.0-only */
/* FM1 Tape OS in the browser: the shared application (src/tapeos.c) compiled to WebAssembly with clang
 * (no Emscripten, no libc), run in an AudioWorklet (web/worklet.js). As in Felucca's browser build (whose
 * design this follows: the device clock driven by the audio the worklet asks for), the main loop runs every
 * millisecond of device time, so timing in the browser is the device's, not the page's.
 *   platform/web/build.sh  ->  build/web/tapeos.wasm (+ the page) */
#include <stdint.h>

/* the compiler may call these for struct copies and clears (wasm32, -mbulk-memory makes most of them
 * memory.copy / memory.fill instructions) */
void *memcpy(void *d, const void *s, unsigned long n)
{
    uint8_t *dp = d;
    const uint8_t *sp = s;
    while (n--) *dp++ = *sp++;
    return d;
}
void *memmove(void *d, const void *s, unsigned long n)
{
    uint8_t *dp = d;
    const uint8_t *sp = s;
    if (dp < sp) while (n--) *dp++ = *sp++;
    else { dp += n; sp += n; while (n--) *--dp = *--sp; }
    return d;
}
void *memset(void *d, int c, unsigned long n)
{
    uint8_t *dp = d;
    while (n--) *dp++ = (uint8_t)c;
    return d;
}

#include "tapeos.c"

#define WEB_MAXN 512u
static float out_l[WEB_MAXN], out_r[WEB_MAXN];
static int32_t mix[2u * WEB_MAXN];
static uint32_t frames, ms_done;

#define EXPORT(n) __attribute__((export_name(n)))

EXPORT("web_init") void web_init(void) { frames = 0; ms_done = 0; tos_init(); }

/* render n frames (<= 512): the audio, then the main loop for every millisecond they cover */
EXPORT("web_render") void web_render(uint32_t n)
{
    uint32_t i, ms;
    if (n > WEB_MAXN) n = WEB_MAXN;
    for (i = 0; i < 2u * n; i++) mix[i] = 0;
    tos_audio(mix, n);
    for (i = 0; i < n; i++) {
        out_l[i] = (float)mix[2u * i] * (1.0f / 32768.0f);
        out_r[i] = (float)mix[2u * i + 1u] * (1.0f / 32768.0f);
    }
    frames += n;
    ms = (uint32_t)((uint64_t)frames * 1000u / TOS_FS);
    while (ms_done < ms) tos_ui(++ms_done);
}

EXPORT("web_panel") void web_panel(uint32_t kind, uint32_t id, int32_t v) { tos_panel(kind, id, v); }
EXPORT("web_midi") void web_midi(uint32_t st, uint32_t d1, uint32_t d2) { tos_midi(st, d1, d2); }
EXPORT("web_out_l") float *web_out_l(void) { return out_l; }
EXPORT("web_out_r") float *web_out_r(void) { return out_r; }
EXPORT("web_fb") uint16_t *web_fb(void) { return tos_fb(); }
EXPORT("web_fb_serial") uint32_t web_fb_serial(void) { return tos_fb_serial(); }
EXPORT("web_leds_keys") uint32_t web_leds_keys(void) { return tos_leds_keys(); }
EXPORT("web_leds_buttons") uint32_t web_leds_buttons(void) { return tos_leds_buttons(); }
EXPORT("web_ms") uint32_t web_ms(void) { return ms_done; }
/* for the page's status line and the browser tests */
EXPORT("web_screen") uint32_t web_screen(void) { return ui.scr; }
EXPORT("web_playing") uint32_t web_playing(void) { return tp.playing | (uint32_t)tp.armed << 1; }
EXPORT("web_tape_pos") uint32_t web_tape_pos(void) { return tp.pos; }
EXPORT("web_undo_levels") uint32_t web_undo_levels(void) { return (uint32_t)(tp.cur - tp.base) | (uint32_t)(tp.top - tp.cur) << 8; }
EXPORT("web_events") uint32_t web_events(uint32_t k)
{
    uint32_t i, n = 0;
    if (k >= NTRACK) return 0;
    for (i = 0; i < tt[k].n; i++) n += (uint32_t)tev_visible(&tt[k].ev[i]);
    return n;
}
EXPORT("web_peak") int32_t web_peak(void) { return mx_peak_master; }
