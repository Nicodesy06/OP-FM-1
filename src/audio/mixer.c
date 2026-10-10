/* SPDX-License-Identifier: GPL-3.0-only */
/* MIXER: four channels (one per tape track: what plays on a track, live or from the tape, sounds through its
 * channel), a ping-pong delay as the send effect, the master. Meters and the scope are measured on the real
 * signal, block by block, for the screen. */

#define DLY_MAX 22050u           /* 0.5 s per side */
static int16_t dly[2][DLY_MAX] TOS_POOL;
static uint32_t dly_w;
static int32_t dly_lp[2];

static uint8_t mx_vol[NTRACK] = {100, 100, 100, 100};
static int8_t mx_pan[NTRACK];    /* -64..63 */
static uint8_t mx_mute[NTRACK];
static uint8_t mx_master = 100;
static uint8_t fx_time = 60, fx_fdbk = 50, fx_mix = 0, fx_tone = 80;

static int32_t mx_peak[NTRACK];  /* Q15, peak with a release, for the meters */
static int32_t mx_peak_master;
#define SCOPE_N 256u
static int16_t scope[SCOPE_N];   /* the selected channel's last samples */
static uint32_t scope_w;

static int32_t vol_q15(uint32_t v) { return (int32_t)((v * v * 2) + v * 2); }   /* 0..127 -> ~0..32512, square law */

static void mix_block(int32_t bus[NTRACK][CTL], int32_t *out, uint32_t sel)
{
    uint32_t c, i;
    int32_t L[CTL], R[CTL], send[CTL];
    int32_t mg = vol_q15(mx_master);
    uint32_t dtime = 441u + (uint32_t)fx_time * 170u;      /* 10 .. 500 ms */
    int32_t fb = fx_fdbk * 240, wet = fx_mix * 258, tk = 2000 + fx_tone * 240;
    int32_t pm = 0;
    for (i = 0; i < CTL; i++) L[i] = R[i] = send[i] = 0;
    for (c = 0; c < NTRACK; c++) {
        int32_t g = mx_mute[c] ? 0 : vol_q15(mx_vol[c]);
        int32_t pr = 16384 + mx_pan[c] * 256, pl = 32767 - pr;   /* linear pan */
        int32_t pk = 0;
        for (i = 0; i < CTL; i++) {
            int32_t s = mulq15(bus[c][i], g), a = s < 0 ? -s : s;
            if (a > pk) pk = a;
            L[i] += mulq15(s, pl) << 1;
            R[i] += mulq15(s, pr) << 1;
            send[i] += s;
            if (c == sel) {
                scope[scope_w] = (int16_t)clamp(bus[c][i] >> 1, -32767, 32767);
                scope_w = (scope_w + 1u) & (SCOPE_N - 1u);
            }
        }
        mx_peak[c] = pk > mx_peak[c] ? pk : mx_peak[c] - (mx_peak[c] >> 4);
    }
    for (i = 0; i < CTL; i++) {                            /* the delay: L feeds R feeds L */
        uint32_t r = dly_w >= dtime ? dly_w - dtime : dly_w + DLY_MAX - dtime;
        int32_t yl = dly[0][r], yr = dly[1][r], l, rr;
        dly_lp[0] += ((yl - dly_lp[0]) * tk) >> 15;
        dly_lp[1] += ((yr - dly_lp[1]) * tk) >> 15;
        dly[0][dly_w] = (int16_t)clamp((wet ? send[i] >> 1 : 0) + mulq15(dly_lp[1], fb), -32767, 32767);
        dly[1][dly_w] = (int16_t)clamp(mulq15(dly_lp[0], fb), -32767, 32767);
        if (++dly_w >= DLY_MAX) dly_w = 0;
        l = L[i] + mulq15(yl, wet);
        rr = R[i] + mulq15(yr, wet);
        l = soft_knee(mulq15(l, mg), 24000);
        rr = soft_knee(mulq15(rr, mg), 24000);
        if ((l < 0 ? -l : l) > pm) pm = l < 0 ? -l : l;
        out[2u * i] += l;
        out[2u * i + 1u] += rr;
    }
    mx_peak_master = pm > mx_peak_master ? pm : mx_peak_master - (mx_peak_master >> 4);
}
