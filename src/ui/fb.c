/* SPDX-License-Identifier: GPL-3.0-only */
/* The frame: 240 x 240 RGB565 in RAM (the FM1's pool), drawn by the screens, sent to the panel by the
 * platform (the FM1: the rows that changed; the browser: the whole frame to a canvas). Every primitive
 * clips to the frame. */
#include "font5x7.h"

#undef RGB                       /* the FM1 build includes Dinghy's gfx.c first: its own names */
#undef C_WHITE
#undef C_DIM
#undef C_BLACK

#define RGB(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

/* the palette: black ground, the four encoder colours (blue, green, white, orange) */
#define C_BG     RGB(0, 0, 0)
#define C_FG     RGB(235, 235, 235)
#define C_DIM    RGB(90, 90, 90)
#define C_DARK   RGB(38, 38, 38)
#define C_BLUE   RGB(50, 120, 245)
#define C_GREEN  RGB(40, 200, 110)
#define C_WHITE  RGB(225, 225, 225)
#define C_ORANGE RGB(255, 110, 30)
#define C_RED    RGB(240, 40, 40)
#define C_YELLOW RGB(250, 210, 40)
static const uint16_t KNOB_COLOR[4] = {C_BLUE, C_GREEN, C_WHITE, C_ORANGE};

static uint16_t fb[TOS_W * TOS_H] TOS_POOL;

static uint16_t dim(uint16_t c, int32_t q8)        /* c * q8 / 256, per channel */
{
    uint32_t r = (uint32_t)(c >> 11), g = (uint32_t)(c >> 5) & 63u, b = (uint32_t)c & 31u;
    if (q8 < 0) q8 = 0;
    if (q8 > 256) q8 = 256;
    return (uint16_t)((r * (uint32_t)q8 >> 8) << 11 | (g * (uint32_t)q8 >> 8) << 5 | (b * (uint32_t)q8 >> 8));
}

static void fb_clear(uint16_t c)
{
    uint32_t i;
    for (i = 0; i < TOS_W * TOS_H; i++) fb[i] = c;
}

static void pset(int32_t x, int32_t y, uint16_t c)
{
    if ((uint32_t)x < TOS_W && (uint32_t)y < TOS_H) fb[y * TOS_W + x] = c;
}

static void fill_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    int32_t i, j, x1 = x + w, y1 = y + h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > TOS_W) x1 = TOS_W;
    if (y1 > TOS_H) y1 = TOS_H;
    for (j = y; j < y1; j++)
        for (i = x; i < x1; i++) fb[j * TOS_W + i] = c;
}

static void rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    fill_rect(x, y, w, 1, c);
    fill_rect(x, y + h - 1, w, 1, c);
    fill_rect(x, y, 1, h, c);
    fill_rect(x + w - 1, y, 1, h, c);
}

static void line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)
{
    int32_t dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int32_t dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int32_t err = dx + dy, n = 0;
    for (;;) {
        int32_t e2;
        pset(x0, y0, c);
        if ((x0 == x1 && y0 == y1) || ++n > 2000) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void circle(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    int32_t x = r, y = 0, err = 1 - r;
    while (x >= y) {
        pset(cx + x, cy + y, c); pset(cx + y, cy + x, c); pset(cx - y, cy + x, c); pset(cx - x, cy + y, c);
        pset(cx - x, cy - y, c); pset(cx - y, cy - x, c); pset(cx + y, cy - x, c); pset(cx + x, cy - y, c);
        y++;
        if (err < 0) err += 2 * y + 1;
        else { x--; err += 2 * (y - x) + 1; }
    }
}

static void fill_circle(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    int32_t y;
    for (y = -r; y <= r; y++) {
        int32_t x = 0;
        while ((x + 1) * (x + 1) + y * y <= r * r) x++;
        fill_rect(cx - x, cy + y, 2 * x + 1, 1, c);
    }
}

/* sin / cos of a turn in 1/1024 (Q15), from Felucca's SINE table */
static int32_t isin(uint32_t a) { return SINE[a & 1023u]; }
static int32_t icos(uint32_t a) { return SINE[(a + 256u) & 1023u]; }

static uint32_t txt_w(const char *s, int32_t sc) { uint32_t n = 0; while (*s++) n++; return n ? n * 6u * (uint32_t)sc - (uint32_t)sc : 0u; }

static int32_t text(int32_t x, int32_t y, const char *s, uint16_t c, int32_t sc)
{
    for (; *s; s++) {
        uint32_t ch = (uint32_t)(uint8_t)*s, r, b;
        if (ch >= 'a' && ch <= 'z') ch -= 32u;
        if (ch < 32u || ch > 95u) ch = '?';
        for (r = 0; r < 7u; r++) {
            uint32_t bits = FONT5X7[ch - 32u][r];
            for (b = 0; b < 5u; b++)
                if (bits >> (4u - b) & 1u) fill_rect(x + (int32_t)b * sc, y + (int32_t)r * sc, sc, sc, c);
        }
        x += 6 * sc;
    }
    return x;
}

static void text_c(int32_t cx, int32_t y, const char *s, uint16_t c, int32_t sc)
{
    text(cx - (int32_t)txt_w(s, sc) / 2, y, s, c, sc);
}

/* small integer to text (no printf on the target) */
static char *itoa_s(int32_t v, char *buf)
{
    char t[12];
    uint32_t n = 0, u = v < 0 ? (uint32_t)-v : (uint32_t)v;
    char *p = buf;
    do { t[n++] = (char)('0' + u % 10u); u /= 10u; } while (u);
    if (v < 0) *p++ = '-';
    while (n) *p++ = t[--n];
    *p = 0;
    return buf;
}
