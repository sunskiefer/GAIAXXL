/* GAIAXXL -- the oscillators: seven waves, three variations each.
 *
 *            A                         B                                   C
 * SAW        polyBLEP saw              Korg's band-limited saw tables      two saws an octave apart (bright)
 * SQR        polyBLEP square           Korg's band-limited square tables   square + sub-octave square
 * PW-SQR     pulse, PW 50..98 %        pulse + sub-octave pulse            two pulses, PW opening both ways
 * TRI        polyBLAMP triangle        Korg wave bank A (PW morphs 1..7)   Korg wave bank B (PW morphs 0..15)
 * SINE       sine                      Korg wave bank C (PW morphs 0..13)  two-operator FM (PW = index)
 * NOISE      white                     pink (Korg's filter)                red (brown)
 * S-SAW      seven detuned saws        seven Korg table saws, wider        seven saws + a sub-octave saw
 *            (PW: the spread on every super saw)
 * The Korg tables are logue-sdk's (websim/dsp), BSD-3-Clause, vendored in src/korg/. */
#include <math.h>
#include "gaia.h"
#include "korg/wavetable_lut.h"
#include "korg/osc_waves.h"

/* ---------------------------------------------------------------- helpers */
static inline float lin(float fr, float a, float b) { return a + fr * (b - a); }

static inline float blep(float t, float dt)
{
    if (t < dt) { const float x = t / dt; return x + x - x * x - 1.f; }
    if (t > 1.f - dt) { const float x = (t - 1.f) / dt; return x * x + x + x + 1.f; }
    return 0.f;
}
static inline float blamp(float t, float dt)
{
    if (t < dt) { const float x = t / dt - 1.f; return -(1.f / 3.f) * x * x * x; }
    if (t > 1.f - dt) { const float x = (t - 1.f) / dt + 1.f; return (1.f / 3.f) * x * x * x; }
    return 0.f;
}
static inline float saw(float t, float dt) { return 2.f * t - 1.f - blep(t, dt); }
static inline float pulse(float t, float dt, float pw)
{
    float t2 = t + 1.f - pw;
    if (t2 >= 1.f) t2 -= 1.f;
    return (t < pw ? 1.f : -1.f) + blep(t, dt) - blep(t2, dt);
}
static inline float tri(float t, float dt)
{
    float t2 = t + 0.5f;
    if (t2 >= 1.f) t2 -= 1.f;
    float v = t < 0.5f ? 4.f * t - 1.f : 3.f - 4.f * t;
    return v + 4.f * dt * (blamp(t, dt) - blamp(t2, dt));
}
static inline float wrap1(float p) { return p >= 1.f ? p - (float)(int)p : p; }

/* Korg's band-limited half-wave tables (logue-sdk osc_api.h osc_bl2_sawf / osc_bl2_sqrf), fractional table index */
static inline float korg_bl(const float *lut, float x, float idx)
{
    const float x0f = 2.f * x * 128.f;
    const uint32_t x0p = (uint32_t)x0f;
    uint32_t x0 = x0p, x1 = x0p + 1;
    float sign = 1.f;
    if (x0p >= 128) { x0 = 128 - (x0p & 127); x1 = x0 - 1; sign = -1.f; }
    int i = (int)idx;
    if (i > 5) i = 5;
    const float *wt = lut + i * 129, fr = x0f - (float)x0p;
    const float y0 = sign * lin(fr, wt[x0], wt[x1]);
    wt += 129;
    const float y1 = sign * lin(fr, wt[x0], wt[x1]);
    return lin(idx - (float)i, y0, y1);
}
static inline float korg_sine(float x)
{
    const float x0f = 2.f * x * 128.f;
    const uint32_t x0p = (uint32_t)x0f;
    const uint32_t x0 = x0p & 127, x1 = (x0 + 1) & 127;
    const float y = lin(x0f - (float)x0p, wt_sine_lut_f[x0], wt_sine_lut_f[x1]);
    return x0p < 128 ? y : -y;
}
/* the table index for a note (logue-sdk osc_api.cpp si_wt_note_offset_frac) */
static float korg_idx(float note, const uint8_t *notes)
{
    int i = 0;
    while (i < 6 && notes[i] < note) i++;
    const float prev = i > 0 ? notes[i - 1] : 0;
    float r = (float)i + (note - prev) / (float)(notes[i] - prev);
    return r > 6.f ? 6.f : r < 0.f ? 0.f : r;
}
/* a morph through a wave bank (logue-sdk osc_wave_scanf, full-period 128-sample waves) */
static inline float bank_scan(const float *const *bank, int cnt, float pos, float x)
{
    float f = pos * (float)(cnt - 1);
    int i = (int)f;
    if (i >= cnt - 1) { i = cnt - 2; f = (float)(cnt - 1); }
    const float x0f = x * 128.f;
    const uint32_t x0 = (uint32_t)x0f & 127, x1 = (x0 + 1) & 127;
    const float fr = x0f - (float)(uint32_t)x0f;
    const float a = lin(fr, bank[i][x0], bank[i][x1]), b = lin(fr, bank[i + 1][x0], bank[i + 1][x1]);
    return lin(f - (float)i, a, b);
}

static const float SS_OFF[NSAW] = {-0.11002313f, -0.06288439f, -0.01952356f, 0.f, 0.01991221f, 0.06216538f, 0.10745242f};

/* each wave's level, matched by ear and by RMS (test/engine_test.c prints them): every wave and variation about as loud */
static const float WGAIN[N_WAVE][3] = {
    {1.20f, 1.05f, 1.35f}, {0.77f, 0.85f, 1.00f}, {0.77f, 0.90f, 1.60f}, {1.20f, 0.87f, 1.55f},
    {1.00f, 1.60f, 1.00f}, {1.45f, 0.75f, 1.10f}, {2.00f, 1.75f, 1.85f}};

void osc_init(void) {}

void osc_reset(ToneSt *s, uint32_t seed)
{
    int i;
    s->nseed = seed | 1;
    for (i = 0; i < NSAW; i++) s->ph[i] = xf(&s->nseed);   /* the super saw's phases: free */
    s->ph[3] = 0.f;
    s->ph2 = 0.f;
    for (i = 0; i < 6; i++) s->pink[i] = 0.f;
    s->red = 0.f;
    s->triInt = 0.f;
}

static inline float white(ToneSt *s) { return xf(&s->nseed) * 2.f - 1.f; }

/* one sample of the oscillator. pw: 0..1 (PW plus its modulation); note: the MIDI note (for Korg's tables).
 * s->inc is this chunk's phase increment. Sets s->wrapped when the main phase wrapped (hard sync). */
float osc_tick(ToneSt *s, int wave, int var, float pw, float note)
{
    const float dt = s->inc;
    float t = s->ph[3], out = 0.f;
    t += dt;
    s->wrapped = t >= 1.f;
    if (s->wrapped) t -= 1.f;
    s->ph[3] = t;
    switch (wave) {
    case W_SAW:
        if (var == 0) out = saw(t, dt);
        else if (var == 1) out = korg_bl(wt_saw_lut_f, t, korg_idx(note, wt_saw_notes));
        else {
            float t2 = wrap1(t * 2.f);
            out = 0.62f * saw(t, dt) + 0.48f * saw(t2, dt * 2.f > 0.5f ? 0.5f : dt * 2.f);
        }
        break;
    case W_SQR:
        if (var == 0) out = pulse(t, dt, 0.5f);
        else if (var == 1) out = korg_bl(wt_sqr_lut_f, t, korg_idx(note, wt_sqr_notes));
        else {
            s->ph2 += dt * 0.5f;
            if (s->ph2 >= 1.f) s->ph2 -= 1.f;
            out = 0.6f * pulse(t, dt, 0.5f) + 0.5f * pulse(s->ph2, dt * 0.5f, 0.5f);
        }
        break;
    case W_PWSQR: {
        const float w = 0.5f + 0.48f * clampf(pw, 0.f, 1.f);
        if (var == 0) out = pulse(t, dt, w);
        else if (var == 1) {
            s->ph2 += dt * 0.5f;
            if (s->ph2 >= 1.f) s->ph2 -= 1.f;
            out = 0.62f * pulse(t, dt, w) + 0.48f * pulse(s->ph2, dt * 0.5f, w);
        } else {
            float t2 = t + 0.37f;
            if (t2 >= 1.f) t2 -= 1.f;
            out = 0.55f * pulse(t, dt, w) + 0.55f * pulse(t2, dt, 1.f - w + 0.02f);
        }
        break;
    }
    case W_TRI:
        if (var == 0) out = tri(t, dt);
        else if (var == 1) out = bank_scan(wavesA, 7, 0.1667f + 0.8333f * clampf(pw, 0, 1), t) * 1.05f;
        else out = bank_scan(wavesB, k_waves_b_cnt, clampf(pw, 0, 1), t);
        break;
    case W_SINE:
        if (var == 0) out = korg_sine(t);
        else if (var == 1) out = bank_scan(wavesC, k_waves_c_cnt, clampf(pw, 0, 1), t);
        else {   /* FM: a modulator an octave up, PW the index */
            s->ph2 += dt * 2.f;
            if (s->ph2 >= 1.f) s->ph2 -= 1.f;
            const float idx = 0.15f + 2.6f * clampf(pw, 0, 1);
            float p = t + idx * 0.159155f * korg_sine(s->ph2);
            p -= floorf(p);
            out = korg_sine(p);
        }
        break;
    case W_NOISE: {
        const float w = white(s);
        if (var == 0) out = w * 0.9f;
        else if (var == 1) {   /* pink: Korg's three first-order sections (osc_api.cpp NoiseFlt::PinkFilter) */
            const float xn = 0.9f * w;
            float acc = xn + s->pink[0] * -0.984436f - s->pink[3] * -0.995728f;
            s->pink[0] = xn;
            s->pink[3] = acc;
            acc += s->pink[1] * -0.833923f - s->pink[4] * -0.947906f;
            s->pink[1] = s->pink[3];
            s->pink[4] = acc;
            acc += s->pink[2] * -0.075684f - s->pink[5] * -0.535675f;
            s->pink[2] = s->pink[4];
            s->pink[5] = acc;
            out = s->pink[5] * 0.83516f * 2.2f;
        } else {   /* red (brown): a leaky integrator (-6 dB per octave above ~35 Hz), its rumble below 20 Hz taken out */
            s->red = 0.995f * s->red + 0.06f * w;
            out = s->red - s->triInt;
            s->triInt += 0.0028f * out;      /* the DC blocker's follower */
            out *= 2.2f;
        }
        break;
    }
    case W_SSAW: default: {
        const float spread = 0.18f + 0.82f * clampf(pw, 0, 1);
        const float side = -0.73764f * spread * spread + 1.2841f * spread + 0.044372f;
        const float mid = -0.55366f * spread + 0.99785f;
        const float det = spread * spread * (var == 1 ? 1.35f : 1.f);
        const float idx = var == 1 ? korg_idx(note, wt_saw_notes) : 0.f;
        float acc = 0.f;
        int i;
        for (i = 0; i < NSAW; i++) {
            float d = dt * (1.f + SS_OFF[i] * det), p;
            if (i == 3) p = t;
            else {
                p = s->ph[i] + d;
                if (p >= 1.f) p -= 1.f;
                s->ph[i] = p;
            }
            const float v = var == 1 ? korg_bl(wt_saw_lut_f, p, idx) : saw(p, d);
            acc += v * (i == 3 ? mid : side);
        }
        out = acc * 0.36f;
        if (var == 2) {
            s->ph2 += dt * 0.5f;
            if (s->ph2 >= 1.f) s->ph2 -= 1.f;
            out = out * 0.85f + 0.45f * saw(s->ph2, dt * 0.5f);
        }
        break;
    }
    }
    return out * WGAIN[wave][var];
}

/* hard sync: the slave restarts with the master */
void osc_sync(ToneSt *s)
{
    s->ph[3] = 0.f;
    s->ph2 = 0.f;
}
