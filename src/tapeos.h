/* SPDX-License-Identifier: GPL-3.0-only */
/* FM1 Tape OS: the public interface of the shared application.
 *
 * The same C code runs in three places, each with a thin platform layer:
 *   firmware/fm1_tapeos.c   M-VAVE FM1 (Dinghy platform: audio ISR, panel scan, LCD, LEDs)
 *   platform/web/web.c      WebAssembly in an AudioWorklet (the browser simulator)
 *   platform/host/host.c    the computer (tests: screens as PPM, sound as WAV)
 *
 * A platform does four things:
 *   tos_init()                      once
 *   tos_audio(out, n)               from the audio callback: ADD n stereo frames (L R L R, Q15 full scale)
 *   tos_panel(kind, id, value)      for every physical control change (the FM1's controls, see below)
 *   tos_ui(ms)                      from the main loop, with the milliseconds since boot: input timing,
 *                                   then the screen is redrawn into tos_fb() when something changed
 * Everything that changes sound or the tape goes from tos_panel / tos_ui to the audio side through one
 * command queue (single writer each side), so the audio callback may interrupt the main loop at any time.
 *
 * Integers only (the FM1 has no FPU and no 64-bit division). */
#pragma once
#include <stdint.h>

#define TOS_FS 44100u
#define TOS_W 240
#define TOS_H 240

/* ---- the physical controls of the FM1 (ids as Dinghy's hal/fm1_input.h numbers them) */
enum {
    TOS_BTN_OCT_DOWN, TOS_BTN_OCT_UP, TOS_BTN_FX, TOS_BTN_SEL, TOS_BTN_ENV, TOS_BTN_LFO, TOS_BTN_EDIT,
    TOS_BTN_GLO, TOS_BTN_HOME, TOS_BTN_SAVE, TOS_BTN_ARP, TOS_BTN_SEQ, TOS_BTN_PLAY, TOS_BTN_REC,
    TOS_NBTN
};
enum { TOS_ENC_SELECT, TOS_ENC_ALGORITHM, TOS_ENC_KNOB1, TOS_ENC_KNOB2, TOS_ENC_KNOB3, TOS_ENC_KNOB4,
       TOS_ENC_PRESETS, TOS_NENC };
#define TOS_NKEYS 27u            /* key 0 = F3 .. key 26 = G5 */

enum { TOS_PE_BTN_DOWN, TOS_PE_BTN_UP, TOS_PE_KEY_DOWN, TOS_PE_KEY_UP, TOS_PE_ENC };

void tos_init(void);
void tos_audio(int32_t *out, uint32_t n);
void tos_panel(uint32_t kind, uint32_t id, int32_t value);
void tos_ui(uint32_t ms);
void tos_midi(uint32_t status, uint32_t d1, uint32_t d2);   /* MIDI in (USB-MIDI on the FM1, Web MIDI) */

void tos_cpu(uint32_t permille);  /* the platform's measured audio load (the FM1: time in the ISR / its period) */

uint16_t *tos_fb(void);          /* 240 x 240 RGB565, native byte order */
uint32_t tos_fb_serial(void);    /* bumps when the frame changed */
uint32_t tos_leds_keys(void);    /* bit k: key k lit */
uint32_t tos_leds_buttons(void); /* bit TOS_BTN_*: button lit */
