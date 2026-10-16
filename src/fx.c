/* GAIAXXL -- the effects, in the GAIA's order: DISTORTION (DIST / FUZZ / BIT CRASH) -> LOW BOOST -> FLANGER (FLANGER /
 * PHASER / PITCH SHIFT) -> DELAY (DELAY / PAN DELAY) -> REVERB (ROOM / HALL / PLATE). Written for GAIAXXL.
 * The reverb is an eight-line feedback delay network (Jot), the flanger a modulated delay with feedback, the phaser six
 * first-order all-passes, the pitch shifter two crossfaded rotating taps. */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "gaia.h"

#define PI_F 3.14159265f
#define DLY_LEN 131072          /* 2.97 s */
#define FLG_LEN 4096
#define PS_LEN 4096
#define RV_LINES 8
#define RV_MAX 8192

typedef struct { float b0, b1, b2, a1, a2, z1, z2; } Biquad;

struct Fx {
    /* distortion */
    float dtone[2], crushHold[2];
    int crushCount;
    /* low boost */
    Biquad boost[2];
    /* flanger / phaser / pitch shifter */
    float *flg[2];
    int flgW;
    float flgPh, flgFb[2];
    float ap[2][6], phFb[2];
    float *ps[2];
    int psW;
    float psPh;
    /* delay */
    float *dl[2];
    int dlW;
    float dlLp[2];
    float dlTime;                 /* smoothed, in samples */
    /* reverb */
    float *rv[RV_LINES];
    int rvLen[RV_LINES], rvW[RV_LINES];
    float rvLp[RV_LINES];
    float *pre;
    int preW;
    float rvAp[4][1024];
    int rvApW[4];
    int lastRevType;
};

Fx *fx_new(void)
{
    Fx *f = calloc(1, sizeof *f);
    int i;
    if (!f) return NULL;
    for (i = 0; i < 2; i++) {
        f->flg[i] = calloc(FLG_LEN, sizeof(float));
        f->ps[i] = calloc(PS_LEN, sizeof(float));
        f->dl[i] = calloc(DLY_LEN, sizeof(float));
    }
    for (i = 0; i < RV_LINES; i++) f->rv[i] = calloc(RV_MAX, sizeof(float));
    f->pre = calloc(8192, sizeof(float));
    for (i = 0; i < 2; i++)
        if (!f->flg[i] || !f->ps[i] || !f->dl[i]) { fx_free(f); return NULL; }
    for (i = 0; i < RV_LINES; i++) if (!f->rv[i]) { fx_free(f); return NULL; }
    if (!f->pre) { fx_free(f); return NULL; }
    f->lastRevType = -1;
    return f;
}

void fx_free(Fx *f)
{
    int i;
    if (!f) return;
    for (i = 0; i < 2; i++) { free(f->flg[i]); free(f->ps[i]); free(f->dl[i]); }
    for (i = 0; i < RV_LINES; i++) free(f->rv[i]);
    free(f->pre);
    free(f);
}

void fx_clear(Fx *f)
{
    int i;
    for (i = 0; i < 2; i++) {
        memset(f->flg[i], 0, FLG_LEN * sizeof(float));
        memset(f->ps[i], 0, PS_LEN * sizeof(float));
        memset(f->dl[i], 0, DLY_LEN * sizeof(float));
    }
    for (i = 0; i < RV_LINES; i++) memset(f->rv[i], 0, RV_MAX * sizeof(float));
    memset(f->pre, 0, 8192 * sizeof(float));
    memset(f->rvAp, 0, sizeof f->rvAp);
    memset(f->ap, 0, sizeof f->ap);
    memset(f->rvLp, 0, sizeof f->rvLp);
    f->flgFb[0] = f->flgFb[1] = f->phFb[0] = f->phFb[1] = f->dlLp[0] = f->dlLp[1] = 0.f;
}

/* ---------------------------------------------------------------- helpers */
static inline float sat(float x) { return x > 3.f ? 1.f : x < -3.f ? -1.f : x * (27.f + x * x) / (27.f + 9.f * x * x); }

static void lowshelf(Biquad *b, float hz, float db)
{
    const float A = powf(10.f, db / 40.f), w = 2.f * PI_F * hz / SR, c = cosf(w), s = sinf(w);
    const float al = s / 2.f * sqrtf(2.f), sa = 2.f * sqrtf(A) * al;
    const float a0 = (A + 1) + (A - 1) * c + sa;
    b->b0 = A * ((A + 1) - (A - 1) * c + sa) / a0;
    b->b1 = 2 * A * ((A - 1) - (A + 1) * c) / a0;
    b->b2 = A * ((A + 1) - (A - 1) * c - sa) / a0;
    b->a1 = -2 * ((A - 1) + (A + 1) * c) / a0;
    b->a2 = ((A + 1) + (A - 1) * c - sa) / a0;
}
static inline float bq(Biquad *b, float x)
{
    const float y = b->b0 * x + b->z1;
    b->z1 = b->b1 * x - b->a1 * y + b->z2;
    b->z2 = b->b2 * x - b->a2 * y;
    return y;
}
static inline float rd(const float *buf, int len, float pos)   /* a fractional read, pos samples behind w */
{
    int i = (int)pos;
    const float fr = pos - (float)i;
    const float a = buf[i & (len - 1)], b = buf[(i + 1) & (len - 1)];
    return a + fr * (b - a);
}

static const float SYNC_WN[N_SYNC_NOTE] = {16, 12, 8, 4, 2, 1, 0.75f, 2.f / 3, 0.5f, 0.375f, 1.f / 3, 0.25f, 0.1875f,
                                           1.f / 6, 0.125f, 0.09375f, 1.f / 12, 0.0625f, 1.f / 24, 0.03125f};

/* the reverb's lines (samples, mutually prime-ish), per type */
static const int RV_LENS[3][RV_LINES] = {
    {601, 683, 757, 863, 947, 1033, 1117, 1201},           /* ROOM */
    {1687, 1949, 2213, 2459, 2731, 3011, 3301, 3593},       /* HALL */
    {953, 1103, 1279, 1453, 1619, 1801, 1993, 2179},        /* PLATE */
};
static const int RV_PRE[3] = {220, 1100, 40};
static const int AP_LENS[4] = {142, 107, 379, 277};

/* ---------------------------------------------------------------- the chain */
void fx_process(Fx *f, const Gaia *g, float *L, float *R, int n)
{
    const float *v = g->v;
    int i, c;
    if (v[P_FX_ON] < 0.5f) return;
    /* ---- distortion */
    {
        const int type = (int)v[P_DIST_TYPE];
        if (type) {
            const float drive = clampf(v[P_DIST_DRIVE] / 127.f + g->beamSp[24] * 0.5f, 0, 1);
            const float tone = v[P_DIST_TONE] / 127.f, lvl = v[P_DIST_LEVEL] / 127.f * 1.3f;
            const float lpc = 1.f - expf(-2.f * PI_F * (700.f * powf(24.f, tone)) / SR);
            if (type == 3) {   /* BIT CRASH: DRIVE the bits (16 .. 2), TONE the rate (full .. 1/48) */
                const float bits = 16.f - drive * 14.f, q = powf(2.f, bits - 1.f);
                const int hold = 1 + (int)((1.f - tone) * (1.f - tone) * 47.f);
                for (i = 0; i < n; i++) {
                    if (++f->crushCount >= hold) {
                        f->crushCount = 0;
                        f->crushHold[0] = roundf(clampf(L[i], -1, 1) * q) / q;
                        f->crushHold[1] = roundf(clampf(R[i], -1, 1) * q) / q;
                    }
                    L[i] = f->crushHold[0] * lvl;
                    R[i] = f->crushHold[1] * lvl;
                }
            } else {
                const float pre = 1.f + drive * drive * (type == 2 ? 70.f : 30.f);
                const float post = lvl / (0.6f + 0.4f * sqrtf(pre) * 0.35f + 0.4f);
                for (i = 0; i < n; i++) {
                    float x[2] = {L[i] * pre, R[i] * pre};
                    for (c = 0; c < 2; c++) {
                        float y;
                        if (type == 1) y = sat(x[c]);
                        else {   /* FUZZ: asymmetric, with a rectified octave */
                            const float a = x[c] > 0 ? 1.f - expf(-x[c]) : -(1.f - expf(x[c] * 0.55f)) * 0.8f;
                            y = sat(a * 1.3f + 0.35f * fabsf(a) * drive) - 0.12f * drive;
                        }
                        f->dtone[c] += lpc * (y - f->dtone[c]);
                        x[c] = f->dtone[c] * post;
                    }
                    L[i] = x[0];
                    R[i] = x[1];
                }
            }
        }
    }
    /* ---- low boost: +9 dB below 110 Hz */
    if (v[P_BOOST] > 0.5f) {
        if (f->boost[0].b0 == 0.f) { lowshelf(&f->boost[0], 110.f, 9.f); f->boost[1] = f->boost[0]; }
        for (i = 0; i < n; i++) { L[i] = bq(&f->boost[0], L[i]); R[i] = bq(&f->boost[1], R[i]); }
    }
    /* ---- flanger / phaser / pitch shifter */
    {
        const int type = (int)v[P_FLG_TYPE];
        const float rate = 0.03f * powf(330.f, v[P_FLG_RATE] / 127.f), depth = v[P_FLG_DEPTH] / 127.f;
        const float fb = v[P_FLG_FB] / 127.f, mix = v[P_FLG_LEVEL] / 127.f;
        if (type == 1) {
            const float base = 0.0015f * powf(4.f, (v[P_FLG_PITCH] + 24.f) / 48.f) * SR;   /* MANUAL: 1.5 .. 6 ms */
            for (i = 0; i < n; i++) {
                float in[2] = {L[i], R[i]};
                f->flgPh += rate / SR;
                if (f->flgPh >= 1.f) f->flgPh -= 1.f;
                for (c = 0; c < 2; c++) {
                    const float ph = f->flgPh + (c ? 0.25f : 0.f);
                    const float lfo = 0.5f - 0.5f * cosf(2.f * PI_F * ph);
                    const float d = 8.f + base * (1.f - depth * 0.9f * lfo) + depth * lfo * base * 0.2f;
                    const float wet = rd(f->flg[c], FLG_LEN, (float)f->flgW - d + FLG_LEN);
                    f->flg[c][f->flgW & (FLG_LEN - 1)] = in[c] + wet * fb * 0.92f;
                    in[c] = in[c] * (1.f - mix * 0.5f) + wet * mix;
                }
                f->flgW = (f->flgW + 1) & (FLG_LEN - 1);
                L[i] = in[0];
                R[i] = in[1];
            }
        } else if (type == 2) {
            for (i = 0; i < n; i++) {
                float in[2] = {L[i], R[i]};
                f->flgPh += rate / SR;
                if (f->flgPh >= 1.f) f->flgPh -= 1.f;
                for (c = 0; c < 2; c++) {
                    const float ph = f->flgPh + (c ? 0.25f : 0.f);
                    const float lfo = 0.5f - 0.5f * cosf(2.f * PI_F * ph);
                    const float hz = 180.f * powf(30.f, depth * lfo + (1.f - depth) * 0.3f);
                    const float t = tanf(PI_F * hz / SR), a = (t - 1.f) / (t + 1.f);
                    float x = in[c] + f->phFb[c] * fb * 0.85f;
                    int s;
                    for (s = 0; s < 6; s++) {   /* first-order all-pass */
                        const float y = a * x + f->ap[c][s];
                        f->ap[c][s] = x - a * y;
                        x = y;
                    }
                    f->phFb[c] = x;
                    in[c] = in[c] * (1.f - mix * 0.5f) + x * mix;
                }
                L[i] = in[0];
                R[i] = in[1];
            }
        } else if (type == 3) {   /* PITCH SHIFT: MANUAL/PITCH in semitones, FEEDBACK the shifted copies regenerated */
            const float ratio = exp2f(v[P_FLG_PITCH] / 12.f), win = 2205.f;   /* 50 ms */
            const float step = (1.f - ratio) / win;
            for (i = 0; i < n; i++) {
                float in[2] = {L[i], R[i]};
                f->psPh += step;
                if (f->psPh >= 1.f) f->psPh -= 1.f;
                if (f->psPh < 0.f) f->psPh += 1.f;
                for (c = 0; c < 2; c++) {
                    const float p1 = f->psPh, p2 = p1 + 0.5f >= 1.f ? p1 - 0.5f : p1 + 0.5f;
                    const float d1 = 8.f + p1 * win, d2 = 8.f + p2 * win;
                    const float w1 = sinf(PI_F * p1), w2 = sinf(PI_F * p2);
                    const float wet = rd(f->ps[c], PS_LEN, (float)f->psW - d1 + PS_LEN) * w1 * w1
                                    + rd(f->ps[c], PS_LEN, (float)f->psW - d2 + PS_LEN) * w2 * w2;
                    f->ps[c][f->psW & (PS_LEN - 1)] = in[c] + wet * fb * 0.6f;
                    in[c] = in[c] * (1.f - mix * 0.5f) + wet * mix;
                }
                f->psW = (f->psW + 1) & (PS_LEN - 1);
                L[i] = in[0];
                R[i] = in[1];
            }
        }
    }
    /* ---- delay */
    {
        const int type = (int)v[P_DLY_TYPE];
        if (type) {
            float target;
            const float fb = v[P_DLY_FB] / 127.f * 0.95f;
            const float lvl = clampf(v[P_DLY_LEVEL] / 127.f + g->beamSp[29] * 0.4f, 0, 1);
            const float damp = 1.f - expf(-2.f * PI_F * (18000.f * powf(0.045f, v[P_DLY_DAMP] / 127.f)) / SR);
            if (v[P_DLY_SYNC] > 0.5f) {
                const float bpm = g->tempo > 20 ? (float)g->tempo : 120.f;
                target = SYNC_WN[(int)v[P_DLY_NOTE] % N_SYNC_NOTE] * 4.f * 60.f / bpm * SR;
            } else target = (0.001f + powf(v[P_DLY_TIME] / 127.f, 2.f) * 1.299f) * SR;
            target = clampf(target, 16.f, DLY_LEN - 4.f);
            if (f->dlTime <= 0.f) f->dlTime = target;
            for (i = 0; i < n; i++) {
                float a, b;
                f->dlTime += (target - f->dlTime) * 0.0005f;
                a = rd(f->dl[0], DLY_LEN, (float)f->dlW - f->dlTime + DLY_LEN);
                b = rd(f->dl[1], DLY_LEN, (float)f->dlW - f->dlTime + DLY_LEN);
                f->dlLp[0] += damp * (a - f->dlLp[0]);
                f->dlLp[1] += damp * (b - f->dlLp[1]);
                if (type == 2) {   /* PAN DELAY: each echo crosses over */
                    f->dl[0][f->dlW & (DLY_LEN - 1)] = (L[i] + R[i]) * 0.5f + f->dlLp[1] * fb;
                    f->dl[1][f->dlW & (DLY_LEN - 1)] = f->dlLp[0] * fb;
                } else {
                    f->dl[0][f->dlW & (DLY_LEN - 1)] = L[i] + f->dlLp[0] * fb;
                    f->dl[1][f->dlW & (DLY_LEN - 1)] = R[i] + f->dlLp[1] * fb;
                }
                f->dlW = (f->dlW + 1) & (DLY_LEN - 1);
                L[i] += a * lvl;
                R[i] += b * lvl;
            }
        } else f->dlTime = 0.f;
    }
    /* ---- reverb */
    {
        const int type = (int)v[P_REV_TYPE];
        if (type) {
            const int ty = type - 1;
            const float rt = 0.25f * powf(40.f, v[P_REV_TIME] / 127.f);    /* RT60: 0.25 .. 10 s */
            const float lvl = clampf(v[P_REV_LEVEL] / 127.f + g->beamSp[29] * 0.4f, 0, 1);
            const float damp = 1.f - expf(-2.f * PI_F * (16000.f * powf(0.06f, v[P_REV_DAMP] / 127.f)) / SR);
            float gain[RV_LINES];
            int k;
            if (f->lastRevType != ty) {
                for (k = 0; k < RV_LINES; k++) { f->rvLen[k] = RV_LENS[ty][k]; memset(f->rv[k], 0, RV_MAX * sizeof(float)); }
                f->lastRevType = ty;
            }
            for (k = 0; k < RV_LINES; k++) gain[k] = powf(10.f, -3.f * (float)f->rvLen[k] / (rt * SR));
            for (i = 0; i < n; i++) {
                float x = (L[i] + R[i]) * 0.5f, o[RV_LINES], s = 0.f, outL = 0.f, outR = 0.f;
                int a;
                f->pre[f->preW & 8191] = x;
                x = f->pre[(f->preW - RV_PRE[ty]) & 8191];
                f->preW = (f->preW + 1) & 8191;
                for (a = 0; a < (ty == 2 ? 4 : 2); a++) {   /* input diffusion (PLATE: more) */
                    float *buf = f->rvAp[a];
                    const int len = AP_LENS[a];
                    const float d = buf[f->rvApW[a] % len];
                    const float y = -0.6f * x + d;
                    buf[f->rvApW[a] % len] = x + 0.6f * y;
                    f->rvApW[a] = (f->rvApW[a] + 1) % len;
                    x = y;
                }
                for (k = 0; k < RV_LINES; k++) {
                    o[k] = f->rv[k][(f->rvW[k] + RV_MAX - f->rvLen[k]) % RV_MAX];
                    f->rvLp[k] += damp * (o[k] - f->rvLp[k]);
                    o[k] = f->rvLp[k] * gain[k];
                    s += o[k];
                }
                s *= 2.f / RV_LINES;    /* Householder feedback */
                for (k = 0; k < RV_LINES; k++) {
                    f->rv[k][f->rvW[k] % RV_MAX] = x + o[k] - s;
                    f->rvW[k] = (f->rvW[k] + 1) % RV_MAX;
                    if (k & 1) outR += o[k]; else outL += o[k];
                }
                L[i] += outL * 0.45f * lvl;
                R[i] += outR * 0.45f * lvl;
            }
        }
    }
}
