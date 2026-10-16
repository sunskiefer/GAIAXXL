/* GAIAXXL -- the musical randomizer. RANDOMIZE makes a new patch of the chosen type (PAD, LEAD, BASS, PLUCK, KEYS,
 * BRASS, STRINGS, FX) inside ranges that keep it playable: tones tuned in consonant intervals (unison, octaves, fifths),
 * a little detune for width, envelopes and filters shaped for the type, levels balanced for the number of tones, and
 * effects in tasteful amounts. AMOUNT (0..100) moves the current patch that far toward the new one: 100 is a new patch,
 * 20 a variation of the one playing. The arpeggiator, the beams and the patch's octave are left alone. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "gaia.h"

enum { R_PAD, R_LEAD, R_BASS, R_PLUCK, R_KEYS, R_BRASS, R_STRINGS, R_FX };

static __thread uint32_t *S;   /* the instance's generator, for this call */
static float rf(float lo, float hi) { return lo + (hi - lo) * xf(S); }
static int ri(int lo, int hi) { return lo + (int)(xr(S) % (uint32_t)(hi - lo + 1)); }
static int chance(float p) { return xf(S) < p; }
static int pickw(const int *w, int n)   /* a weighted choice */
{
    int i, sum = 0, r;
    for (i = 0; i < n; i++) sum += w[i];
    r = ri(0, sum - 1);
    for (i = 0; i < n; i++) { if (r < w[i]) return i; r -= w[i]; }
    return 0;
}

/* wave weights per type: SAW SQR PW-SQR TRI SINE NOISE S-SAW */
static const int WAVE_W[8][N_WAVE] = {
    {4, 1, 3, 2, 1, 0, 5},    /* PAD */
    {5, 3, 4, 1, 1, 0, 2},    /* LEAD */
    {5, 4, 3, 1, 2, 0, 1},    /* BASS */
    {4, 3, 3, 3, 2, 0, 1},    /* PLUCK */
    {2, 2, 3, 4, 5, 0, 0},    /* KEYS */
    {7, 1, 3, 0, 0, 0, 2},    /* BRASS */
    {6, 0, 2, 1, 0, 0, 6},    /* STRINGS */
    {2, 2, 2, 2, 2, 4, 2},    /* FX */
};
/* the second and third tones' pitch (semitones), by how musical they stay */
static const int INTERVALS[] = {0, 0, 0, 12, 12, -12, 7, 19, 24, -5};

typedef struct { float aa[2], ad[2], as[2], ar[2], fa[2], fd[2], fs[2], fr[2], cut[2], res[2], fdep[2]; } Shape;
static const Shape SHAPES[8] = {
    /* PAD */     {{55, 85}, {60, 95}, {80, 120}, {65, 95}, {50, 90}, {60, 100}, {40, 90}, {60, 95}, {35, 75}, {0, 50}, {-10, 30}},
    /* LEAD */    {{0, 18}, {40, 80}, {80, 127}, {20, 50}, {0, 20}, {40, 80}, {30, 90}, {20, 50}, {40, 85}, {10, 70}, {10, 40}},
    /* BASS */    {{0, 6}, {40, 75}, {50, 110}, {5, 35}, {0, 8}, {35, 70}, {0, 40}, {5, 35}, {20, 60}, {10, 85}, {20, 50}},
    /* PLUCK */   {{0, 5}, {50, 80}, {0, 15}, {40, 70}, {0, 5}, {35, 70}, {0, 20}, {35, 65}, {25, 60}, {10, 60}, {25, 55}},
    /* KEYS */    {{0, 8}, {60, 90}, {20, 70}, {35, 65}, {0, 8}, {50, 85}, {20, 70}, {35, 65}, {45, 85}, {0, 40}, {5, 30}},
    /* BRASS */   {{18, 40}, {50, 80}, {80, 115}, {25, 50}, {20, 45}, {50, 80}, {50, 95}, {25, 50}, {30, 60}, {0, 35}, {20, 45}},
    /* STRINGS */ {{40, 75}, {55, 85}, {95, 127}, {55, 85}, {35, 70}, {55, 85}, {70, 115}, {55, 85}, {50, 85}, {0, 30}, {0, 20}},
    /* FX */      {{0, 90}, {20, 110}, {0, 127}, {10, 110}, {0, 90}, {20, 110}, {0, 127}, {10, 110}, {20, 110}, {0, 110}, {-50, 60}},
};

static void set(float *p, int k, float v)
{
    const ParamDef *d = &PDEF[k];
    if (d->nopts) { int i = (int)(v + 0.5f); p[k] = (float)(i < 0 ? 0 : i >= d->nopts ? d->nopts - 1 : i); }
    else p[k] = clampf((float)(int)(v + (v < 0 ? -0.5f : 0.5f)), d->min, d->max);
}
#define SETT(p, t, name, v) set(p, TP(t, name), v)

/* the patch's loudness, estimated from its tones (levels, sustain, filter), and PATCH LEVEL set to even it out */
static void normalize(float *p)
{
    float pow2 = 0.f, est;
    int t;
    for (t = 0; t < NTONES; t++) {
        float a, sus, cut, att;
        if (p[TP(t, ON)] < 0.5f) continue;
        a = p[TP(t, LEVEL)] / 127.f;
        a *= a;
        sus = p[TP(t, AS)] / 127.f;
        if (sus < 0.4f) sus = 0.4f + 0.3f * p[TP(t, AD)] / 127.f;
        cut = p[TP(t, CUTOFF)] + (p[TP(t, FDEPTH)] > 0 ? p[TP(t, FDEPTH)] * 0.6f : 0.f);
        att = (int)p[TP(t, FMODE)] == F_LPF ? clampf((cut - 15.f) / 60.f, 0.3f, 1.f) : (int)p[TP(t, FMODE)] == F_BPF ? 0.75f
            : (int)p[TP(t, FMODE)] == F_PKG ? 1.f + 1.4f * p[TP(t, RESO)] / 127.f : 1.f;
        a *= sus * att;
        pow2 += a * a;
    }
    est = pow2 > 0 ? sqrtf(pow2) : 0.62f;
    p[P_LEVEL] = clampf((float)(int)(100.f * sqrtf(0.62f / est)), 64.f, 112.f);
}

static void make(float *p, int type)
{
    const Shape *s = &SHAPES[type];
    int t, ntones, w;
    static const int NT_W[8][3] = {{2, 4, 4}, {4, 4, 2}, {4, 4, 1}, {4, 4, 2}, {5, 4, 1}, {3, 4, 2}, {2, 4, 3}, {3, 3, 3}};
    ntones = 1 + pickw(NT_W[type], 3);
    w = pickw(WAVE_W[type], N_WAVE);
    for (t = 0; t < NTONES; t++) {
        int wave = t == 0 || chance(0.4f) ? w : pickw(WAVE_W[type], N_WAVE);
        const float lvl = ntones == 1 ? rf(95, 112) : ntones == 2 ? rf(80, 98) : rf(68, 86);
        SETT(p, t, ON, t < ntones);
        if (type == R_FX && t == 0 && chance(0.3f)) wave = 5;
        SETT(p, t, WAVE, wave);
        SETT(p, t, VAR, ri(0, 2));
        SETT(p, t, PITCH, t == 0 ? (type == R_BASS && chance(0.4f) ? -12 : 0)
                         : INTERVALS[ri(0, (int)(sizeof INTERVALS / sizeof INTERVALS[0]) - (type == R_FX ? 1 : 2))]);
        SETT(p, t, DETUNE, t == 0 ? rf(-4, 4) : rf(-14, 14) * (type == R_PAD || type == R_STRINGS ? 1.4f : 1.f));
        SETT(p, t, PW, wave == W_PWSQR ? rf(20, 90) : wave == W_SSAW ? rf(30, 100) : rf(0, 70));
        SETT(p, t, PWM, wave == W_PWSQR || wave == W_SSAW ? rf(0, 70) : 0);
        SETT(p, t, PDEPTH, type == R_FX ? rf(-50, 50) : (type == R_BASS || type == R_PLUCK) && chance(0.35f) ? rf(4, 16) : 0);
        SETT(p, t, PA, 0);
        SETT(p, t, PD, rf(20, 60));
        /* the filter */
        SETT(p, t, FMODE, type == R_FX ? ri(0, 4) : chance(0.85f) ? 0 : chance(0.5f) ? 2 : 3);
        SETT(p, t, SLOPE, type == R_BASS || type == R_LEAD ? chance(0.7f) : chance(0.4f));
        SETT(p, t, CUTOFF, rf(s->cut[0], s->cut[1]) + (wave == W_TRI || wave == W_SINE ? 20 : 0));
        SETT(p, t, RESO, rf(s->res[0], s->res[1]));
        SETT(p, t, KF, rf(0, 60));
        SETT(p, t, FA, rf(s->fa[0], s->fa[1]));
        SETT(p, t, FD, rf(s->fd[0], s->fd[1]));
        SETT(p, t, FS, rf(s->fs[0], s->fs[1]));
        SETT(p, t, FR, rf(s->fr[0], s->fr[1]));
        SETT(p, t, FDEPTH, rf(s->fdep[0], s->fdep[1]));
        SETT(p, t, FVELO, rf(0, 30));
        /* the amp */
        SETT(p, t, LEVEL, lvl);
        SETT(p, t, AA, rf(s->aa[0], s->aa[1]));
        SETT(p, t, AD, rf(s->ad[0], s->ad[1]));
        SETT(p, t, AS, rf(s->as[0], s->as[1]));
        SETT(p, t, AR, rf(s->ar[0], s->ar[1]));
        SETT(p, t, AVELO, type == R_PAD || type == R_STRINGS ? rf(0, 15) : rf(10, 40));
        SETT(p, t, PAN, ntones > 1 ? rf(-28, 28) : 0);
        /* the LFO: a gentle vibrato or movement, rarely more */
        SETT(p, t, LSHAPE, type == R_FX ? ri(0, 5) : chance(0.7f) ? 1 : ri(0, 3));
        SETT(p, t, LRATE, rf(55, 90));
        SETT(p, t, LSYNC, type == R_FX ? chance(0.5f) : 0);
        SETT(p, t, LNOTE, ri(8, 17));
        SETT(p, t, LFADE, type == R_LEAD || type == R_STRINGS ? rf(30, 80) : rf(0, 40));
        SETT(p, t, LKEY, chance(0.3f));
        SETT(p, t, LPITCH, type == R_LEAD || type == R_STRINGS ? rf(0, 7) : 0);
        SETT(p, t, LFILTER, type == R_PAD || type == R_FX ? rf(-25, 25) : 0);
        SETT(p, t, LAMP, 0);
        SETT(p, t, LPAN, type == R_PAD ? rf(-20, 20) : 0);
        if (type == R_FX) { SETT(p, t, LPITCH, rf(-30, 30)); SETT(p, t, LAMP, rf(-30, 30)); }
        SETT(p, t, MSHAPE, 1);
        SETT(p, t, MRATE, rf(70, 85));
        SETT(p, t, MPITCH, rf(6, 14));
        SETT(p, t, MFILTER, 0);
        SETT(p, t, MAMP, 0);
        SETT(p, t, MPAN, 0);
    }
    /* common */
    set(p, P_MONO, type == R_LEAD || type == R_BASS ? chance(0.6f) : 0);
    set(p, P_PORTA, type == R_LEAD ? chance(0.5f) : type == R_BASS ? chance(0.25f) : 0);
    set(p, P_PTIME, rf(10, 40));
    set(p, P_MOD, type == R_FX ? ri(0, 2) : type == R_LEAD && chance(0.25f) ? 1 : 0);
    set(p, P_LEVEL, 100);
    set(p, P_FX_ON, 1);
    set(p, P_BOOST, type == R_BASS ? chance(0.5f) : 0);
    /* effects */
    set(p, P_DIST_TYPE, type == R_LEAD || type == R_BASS ? (chance(0.35f) ? ri(1, 2) : 0) : type == R_FX && chance(0.4f) ? ri(1, 3) : 0);
    set(p, P_DIST_DRIVE, rf(20, 70));
    set(p, P_DIST_TONE, rf(60, 110));
    set(p, P_DIST_LEVEL, rf(80, 100));
    set(p, P_FLG_TYPE, type == R_PAD || type == R_STRINGS ? (chance(0.5f) ? ri(1, 2) : 0) : type == R_FX ? ri(0, 3) : chance(0.15f) ? 2 : 0);
    set(p, P_FLG_RATE, rf(10, 45));
    set(p, P_FLG_DEPTH, rf(40, 90));
    set(p, P_FLG_FB, rf(20, 70));
    set(p, P_FLG_PITCH, type == R_FX ? (float)INTERVALS[ri(0, 9)] : 0);
    set(p, P_FLG_LEVEL, rf(40, 80));
    set(p, P_DLY_TYPE, type == R_LEAD || type == R_PLUCK || type == R_FX ? ri(1, 2) : chance(0.3f) ? 1 : 0);
    set(p, P_DLY_SYNC, 1);
    set(p, P_DLY_NOTE, ri(0, 2) ? 14 : 12);
    set(p, P_DLY_FB, rf(25, 60));
    set(p, P_DLY_DAMP, rf(50, 90));
    set(p, P_DLY_LEVEL, rf(25, 55));
    set(p, P_REV_TYPE, type == R_BASS ? (chance(0.3f) ? 1 : 0) : type == R_PAD || type == R_STRINGS ? ri(2, 3) : ri(1, 3));
    set(p, P_REV_TIME, type == R_PAD || type == R_STRINGS ? rf(75, 110) : rf(40, 80));
    set(p, P_REV_DAMP, rf(50, 90));
    set(p, P_REV_LEVEL, type == R_PAD || type == R_STRINGS ? rf(45, 75) : rf(20, 50));
    normalize(p);
}

void random_patch(Gaia *g, int type, float amount)
{
    float p[P_COUNT];
    int k;
    if (type < 0 || type >= N_RND) type = 0;
    amount = clampf(amount, 0.f, 1.f);
    S = &g->rnd;
    xr(S);
    memcpy(p, g->v, sizeof p);
    make(p, type);
    for (k = 0; k < P_COUNT; k++) {
        const ParamDef *d = &PDEF[k];
        if (d->kind != PK_PATCH || k == P_OCTAVE || (k >= P_BEAM1_DEST && k <= P_BEAM2)
            || (k >= P_ARP_ON && k <= P_ARP_VELO))
            continue;
        if (d->nopts) { if (amount >= 1.f || xf(S) < amount) g->v[k] = p[k]; }
        else g->v[k] = (float)(int)(g->v[k] + (p[k] - g->v[k]) * amount + (p[k] >= g->v[k] ? 0.5f : -0.5f));
    }
    if (g->v[P_T1_ON] < 0.5f && g->v[P_T2_ON] < 0.5f && g->v[P_T3_ON] < 0.5f) g->v[P_T1_ON] = 1;   /* never silent */
    snprintf(g->patchName, sizeof g->patchName, "RANDOM %s", RND_NAMES[type]);
    g->dirty = 1;
}

/* the factory's generated patches (patches.c): a type and a seed */
void random_seeded(Gaia *g, int type, uint32_t seed)
{
    uint32_t keep = g->rnd;
    g->rnd = seed;
    S = &g->rnd;
    make(g->v, type);
    g->rnd = keep;
}
