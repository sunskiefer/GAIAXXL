/* GAIAXXL -- the voices: three tones each (OSC, pitch envelope, FILTER with its envelope, AMP with its envelope, LFO,
 * MOD LFO), TONE 1 -> TONE 2 SYNC / RING, mono and portamento, the bender and the modulation lever, the two beams.
 *
 * Mapping of the GAIA's 0..127 values (chosen by ear against the hardware's ranges):
 *   envelope times   0.5 ms .. 12 s (64: 0.8 s)            cutoff   16 Hz .. 19.5 kHz, exponential
 *   LFO rate         0.05 .. 30 Hz                       PORTA    0 .. 2 s (squared)
 *   depths -63..+63  curved (fine near zero): pitch +-12 semitones (LFO) / +-36 (pitch envelope),
 *                    filter +-4 octaves (LFO) / +-8 octaves (envelope) */
#include <math.h>
#include <string.h>
#include "gaia.h"

#define PI_F 3.14159265f

/* ---------------------------------------------------------------- mappings */
static inline float env_secs(float v)   /* 0.5 ms .. 12 s: 32 = 50 ms, 64 = 0.8 s, 96 = 4 s */
{
    const float x = clampf(v, 0, 127) / 127.f;
    return 0.0005f + 12.f * (x * x) * (x * x);
}
static inline float curve(float d, float p) { const float a = fabsf(d) / 63.f; return (d < 0 ? -1.f : 1.f) * powf(a > 1 ? 1 : a, p); }
static inline float cutoff_hz(float v) { return 16.35f * exp2f(clampf(v, 0, 127) * (10.25f / 127.f)); }

float lfo_hz(const Gaia *g, int rate, int sync, int note)
{
    static const float WN[N_SYNC_NOTE] = {16, 12, 8, 4, 2, 1, 0.75f, 2.f / 3, 0.5f, 0.375f, 1.f / 3, 0.25f, 0.1875f,
                                          1.f / 6, 0.125f, 0.09375f, 1.f / 12, 0.0625f, 1.f / 24, 0.03125f};
    if (sync) {
        const float bpm = g->tempo > 20 ? (float)g->tempo : 120.f;
        return bpm / 60.f / (4.f * WN[note < 0 ? 0 : note >= N_SYNC_NOTE ? N_SYNC_NOTE - 1 : note]);
    }
    return 0.05f * powf(600.f, (float)rate / 127.f);
}

/* ---------------------------------------------------------------- the beams: offsets on the tone parameters */
static const int BEAM_TONE_PARAM[N_BEAM] = {
    P_T1_LRATE, P_T1_LFADE, P_T1_LPITCH, P_T1_LFILTER, P_T1_LAMP, P_T1_PITCH, P_T1_DETUNE, P_T1_PWM, P_T1_PW,
    P_T1_PA, P_T1_PD, P_T1_PDEPTH, P_T1_CUTOFF, P_T1_RESO, P_T1_FA, P_T1_FD, P_T1_FS, P_T1_FR, P_T1_FDEPTH,
    P_T1_LEVEL, P_T1_AA, P_T1_AD, P_T1_AS, P_T1_AR, -1, -1, -1, -1, P_T1_KF, -1};
enum { BEAM_FXCTRL = 24, BEAM_PORTA = 25, BEAM_BEND = 26, BEAM_MOD = 27, BEAM_FXLEVEL = 29 };

/* this chunk's tone parameters (g->E): the patch plus the beams */
static void effective(Gaia *g)
{
    float (*E)[TONE_PARAMS] = g->E;
    float *beamSpecial = g->beamSp;
    int t, b, i;
    for (t = 0; t < NTONES; t++)
        for (i = 0; i < TONE_PARAMS; i++) E[t][i] = g->v[TONE_BASE(t) + i];
    memset(g->beamSp, 0, sizeof g->beamSp);
    for (b = 0; b < 2; b++) {
        const float amt = g->v[b ? P_BEAM2 : P_BEAM1] / 127.f;
        const int d = (int)g->v[b ? P_BEAM2_DEST : P_BEAM1_DEST];
        if (amt <= 0.f || d < 0 || d >= N_BEAM) continue;
        if (BEAM_TONE_PARAM[d] >= 0) {
            const int i0 = BEAM_TONE_PARAM[d] - P_T1_ON;
            const ParamDef *pd = &PDEF[BEAM_TONE_PARAM[d]];
            for (t = 0; t < NTONES; t++)
                E[t][i0] = clampf(E[t][i0] + amt * (pd->max - pd->min) * 0.6f, pd->min, pd->max);
        } else beamSpecial[d] += amt;
    }
}
#define EP(t, name) (g->E[t][P_T1_##name - P_T1_ON])

/* ---------------------------------------------------------------- envelopes (ticked once per chunk) */
static inline float approach(float lvl, float target, float secs, int n)
{
    return target + (lvl - target) * expf(-(float)n / (secs * SR + 1.f));
}
static void env_adsr(Env *e, float a, float d, float s, float r, int n)
{
    switch (e->stage) {
    case 1:
        e->lvl = approach(e->lvl, 1.3f, env_secs(a) / 1.466f, n);
        if (e->lvl >= 1.f) { e->lvl = 1.f; e->stage = 2; }
        break;
    case 2: e->lvl = approach(e->lvl, s / 127.f, env_secs(d) / 4.6f, n); break;
    case 3:
        e->lvl = approach(e->lvl, -0.01f, env_secs(r) / 4.6f, n);
        if (e->lvl <= 0.f) { e->lvl = 0.f; e->stage = 0; }
        break;
    default: break;
    }
}
static void env_ad(Env *e, float a, float d, int n)   /* the pitch envelope: up, then back to zero */
{
    if (e->stage == 1) {
        e->lvl = approach(e->lvl, 1.3f, env_secs(a) / 1.466f, n);
        if (e->lvl >= 1.f) { e->lvl = 1.f; e->stage = 2; }
    } else if (e->stage >= 2) {
        e->lvl = approach(e->lvl, 0.f, env_secs(d) / 4.6f, n);
        if (e->lvl < 1e-4f) { e->lvl = 0.f; e->stage = 0; }
    }
}

/* ---------------------------------------------------------------- LFOs */
static float lfo_tick(Lfo *l, int shape, float hz, int n)
{
    const float before = l->ph;
    l->ph += hz * (float)n / SR;
    if (l->ph >= 1.f) {
        l->ph -= (float)(int)l->ph;
        l->sh = xf(&l->rnd) * 2.f - 1.f;
        l->rndFrom = l->rndTo;
        l->rndTo = xf(&l->rnd) * 2.f - 1.f;
    }
    (void)before;
    const float p = l->ph;
    switch (shape) {
    case 0: return p < 0.5f ? 4.f * p - 1.f : 3.f - 4.f * p;              /* TRI */
    case 1: return sinf(2.f * PI_F * p);                                  /* SIN */
    case 2: return 1.f - 2.f * p;                                         /* SAW (down, like a ramp lever) */
    case 3: return p < 0.5f ? 1.f : -1.f;                                 /* SQR */
    case 4: return l->sh;                                                 /* S&H */
    default: {                                                            /* RND: smoothed */
        const float s = p * p * (3.f - 2.f * p);
        return l->rndFrom + (l->rndTo - l->rndFrom) * s;
    }
    }
}

/* ---------------------------------------------------------------- voices */
void gaia_init(Gaia *g)
{
    int i, t;
    memset(g->voice, 0, sizeof g->voice);
    g->rnd = 0x1234567u;
    for (i = 0; i < NVOICES; i++)
        for (t = 0; t < NTONES; t++) {
            osc_reset(&g->voice[i].t[t], 0x9E3779B9u * (uint32_t)(i * 3 + t + 1));
            g->voice[i].t[t].lfo.rnd = 0x51ED27u * (uint32_t)(i * 3 + t + 7);
            g->voice[i].t[t].mlfo.rnd = 0x2545F491u * (uint32_t)(i * 3 + t + 11);
        }
    g->tempo = 120.0;
    g->bend = 0.f;
    g->modWheel = 0.f;
    g->arp.playing = -1;
    g->lastPitch = -1.f;
}

static void voice_start(Gaia *g, Voice *v, int note, int vel, int retrig)
{
    int t;
    const int porta = g->v[P_PORTA] > 0.5f;
    v->note = note;
    v->vel = (float)vel / 127.f;
    v->pitchTarget = (float)note;
    if (!porta || !v->active) {
        /* the glide starts from the last note played (poly portamento as on the GAIA) */
        v->pitch = porta && g->lastPitch >= 0 ? g->lastPitch : (float)note;
    }
    {
        g->lastPitch = (float)note;
    }
    v->gate = 1;
    v->age = ++g->clock;
    if (retrig || !v->active) {
        for (t = 0; t < NTONES; t++) {
            ToneSt *s = &v->t[t];
            if (!v->active) {
                memset(s->s1, 0, sizeof s->s1);
                memset(s->s2, 0, sizeof s->s2);
                s->aenv.lvl = s->fenv.lvl = s->penv.lvl = 0.f;
                s->gl0 = s->gr0 = s->gl1 = s->gr1 = 0.f;
            }
            s->aenv.stage = s->fenv.stage = s->penv.stage = 1;
            s->penv.lvl = 0.f;
            if (TV(g, t, LKEY) > 0.5f) s->lfo.ph = 0.f; else s->lfo.ph = g->gph[t];
            s->mlfo.ph = g->gmph[t];
            s->fade = 0.f;
            if (TV(g, t, WAVE) != W_SSAW) s->ph[3] = 0.f;   /* the single oscillators start at zero (punchy attacks) */
        }
    }
    v->active = 1;
}

static void voice_release(Voice *v)
{
    int t;
    v->gate = 0;
    for (t = 0; t < NTONES; t++) {
        ToneSt *s = &v->t[t];
        if (s->aenv.stage) s->aenv.stage = 3;
        if (s->fenv.stage) s->fenv.stage = 3;
    }
}

static Voice *voice_pick(Gaia *g, int note)
{
    Voice *best = NULL;
    int i;
    for (i = 0; i < NVOICES; i++) if (g->voice[i].active && g->voice[i].note == note) return &g->voice[i];
    for (i = 0; i < NVOICES; i++) if (!g->voice[i].active) return &g->voice[i];
    for (i = 0; i < NVOICES; i++)   /* the oldest released */
        if (!g->voice[i].gate && (!best || g->voice[i].age < best->age)) best = &g->voice[i];
    if (best) return best;
    for (i = 0; i < NVOICES; i++) if (!best || g->voice[i].age < best->age) best = &g->voice[i];
    return best;
}

void synth_key_on(Gaia *g, int note, int vel)
{
    note += (int)g->v[P_OCTAVE] * 12;
    if (note < 0 || note > 127) return;
    if (g->v[P_MONO] > 0.5f) {
        Voice *v = &g->voice[0];
        int i, legato = g->nMono > 0 && v->gate;
        for (i = 0; i < g->nMono; i++)
            if (g->monoStack[i] == note) { memmove(g->monoStack + i, g->monoStack + i + 1, (size_t)(g->nMono - i - 1) * sizeof(int)); g->nMono--; break; }
        if (g->nMono < MAX_HELD) { g->monoStack[g->nMono] = note; g->monoVel[g->nMono] = (float)vel; g->nMono++; }
        for (i = 1; i < NVOICES; i++) if (g->voice[i].active) voice_release(&g->voice[i]);
        if (legato) {   /* legato: the pitch moves, the envelopes go on */
            v->note = note;
            v->pitchTarget = (float)note;
            if (g->v[P_PORTA] < 0.5f) v->pitch = (float)note;
        } else voice_start(g, v, note, vel, 1);
        return;
    }
    voice_start(g, voice_pick(g, note), note, vel, 1);
}

void synth_key_off(Gaia *g, int note)
{
    int i;
    note += (int)g->v[P_OCTAVE] * 12;
    if (g->v[P_MONO] > 0.5f) {
        Voice *v = &g->voice[0];
        for (i = 0; i < g->nMono; i++)
            if (g->monoStack[i] == note) { memmove(g->monoStack + i, g->monoStack + i + 1, (size_t)(g->nMono - i - 1) * sizeof(int)); g->nMono--; break; }
        if (g->nMono > 0) {   /* back to the last key still held */
            const int n2 = g->monoStack[g->nMono - 1];
            v->note = n2;
            v->pitchTarget = (float)n2;
            if (g->v[P_PORTA] < 0.5f) v->pitch = (float)n2;
        } else if (v->note == note && !g->sustain) voice_release(v);
        return;
    }
    if (g->sustain) return;
    for (i = 0; i < NVOICES; i++)
        if (g->voice[i].active && g->voice[i].gate && g->voice[i].note == note) voice_release(&g->voice[i]);
}

void gaia_note_on(Gaia *g, int note, int vel)
{
    if (g->v[P_ARP_ON] > 0.5f) arp_key(g, note, vel, 1);
    else synth_key_on(g, note, vel);
}

void gaia_note_off(Gaia *g, int note)
{
    if (g->v[P_ARP_ON] > 0.5f) arp_key(g, note, 0, 0);
    else synth_key_off(g, note);
}

void gaia_all_off(Gaia *g, int hard)
{
    int i;
    g->nMono = 0;
    arp_reset(g);
    for (i = 0; i < NVOICES; i++) {
        voice_release(&g->voice[i]);
        if (hard) {
            int t;
            g->voice[i].active = 0;
            for (t = 0; t < NTONES; t++) {
                ToneSt *s = &g->voice[i].t[t];
                s->aenv.stage = s->fenv.stage = s->penv.stage = 0;
                s->aenv.lvl = s->fenv.lvl = s->penv.lvl = 0.f;
                s->gl0 = s->gr0 = s->gl1 = s->gr1 = 0.f;
            }
        }
    }
}

/* ---------------------------------------------------------------- the filter: Cytomic's trapezoidal SVF, two stages */
static inline float svf(float *ic1, float *ic2, float g, float k, float v0, float *band, float *high)
{
    const float a1 = 1.f / (1.f + g * (g + k)), a2 = g * a1, a3 = g * a2;
    const float v3 = v0 - *ic2;
    const float v1 = a1 * *ic1 + a2 * v3;
    const float v2 = *ic2 + a2 * *ic1 + a3 * v3;
    *ic1 = 2.f * v1 - *ic1;
    *ic2 = 2.f * v2 - *ic2;
    *band = v1;
    *high = v0 - k * v1 - v2;
    return v2;
}
static inline float sat(float x) { return x > 3.f ? 1.f : x < -3.f ? -1.f : x * (27.f + x * x) / (27.f + 9.f * x * x); }

static inline float filter(ToneSt *s, int mode, int slope24, float g, float k, float x)
{
    float b, h, l;
    if (mode == F_BYPASS) return x;
    if (mode == F_PKG) {
        /* PKG: a peaking filter. The band-pass (unity at the cutoff) is added to the dry signal: gainPk is the boost
         * (+6 dB at RESONANCE 0 up to +24 dB at 127), k the width (wide at 0, narrow at 127). -24 dB runs a second
         * stage: the same peak, steeper skirts. */
        l = svf(&s->s1[0], &s->s2[0], g, k, x, &b, &h);
        x = x + b * k * s->gainPk;
        if (slope24) {
            l = svf(&s->s1[1], &s->s2[1], g, k * 1.6f, x, &b, &h);
            x = x + b * k * 1.6f * s->gainPk * 0.35f;
        }
        return x;
    }
    if (!slope24) {
        l = svf(&s->s1[0], &s->s2[0], g, k, x, &b, &h);
        if (mode == F_LPF) return l;
        if (mode == F_HPF) return h;
        return b * k;
    }
    {   /* -24 dB: two stages (Butterworth spacing), the resonance on the second */
        float b2, h2, x2;
        const float k1 = 1.848f;
        l = svf(&s->s1[0], &s->s2[0], g, k1, x, &b, &h);
        x2 = mode == F_LPF ? l : mode == F_HPF ? h : b * k1;
        l = svf(&s->s1[1], &s->s2[1], g, k, sat(x2), &b2, &h2);
        if (mode == F_LPF) return l;
        if (mode == F_HPF) return h2;
        return b2 * k;
    }
}

/* ---------------------------------------------------------------- the render */
void gaia_render(Gaia *g, float *L, float *R, int n)
{
    int off, t, vi, i;
    memset(L, 0, sizeof(float) * (size_t)n);
    memset(R, 0, sizeof(float) * (size_t)n);
    for (off = 0; off < n; off += CTRL) {
        const int m = n - off < CTRL ? n - off : CTRL;
        const int mod = (int)g->v[P_MOD];
        const float bendSemis = (g->bend >= 0 ? g->v[P_BEND_UP] : g->v[P_BEND_DN]) * g->bend;
        float modAmt, porta;
        arp_tick(g, m);
        effective(g);
        modAmt = clampf(g->modWheel + g->beamSp[BEAM_MOD], 0, 1);
        porta = g->v[P_PORTA] > 0.5f ? clampf(g->v[P_PTIME] / 127.f + g->beamSp[BEAM_PORTA] * 0.6f, 0, 1) : 0.f;
        for (t = 0; t < NTONES; t++) {   /* the free-running LFO phases */
            g->gph[t] += lfo_hz(g, (int)EP(t, LRATE), EP(t, LSYNC) > 0.5f, (int)EP(t, LNOTE)) * (float)m / SR;
            g->gph[t] -= (float)(int)g->gph[t];
            g->gmph[t] += lfo_hz(g, (int)EP(t, MRATE), EP(t, MSYNC) > 0.5f, (int)EP(t, MNOTE)) * (float)m / SR;
            g->gmph[t] -= (float)(int)g->gmph[t];
        }
        for (vi = 0; vi < NVOICES; vi++) {
            Voice *v = &g->voice[vi];
            float pitch, pwv[NTONES], gk[NTONES], kk[NTONES], dgl[NTONES], dgr[NTONES];
            int alive = 0, on[NTONES], wave[NTONES], var[NTONES], fm[NTONES], sl[NTONES];
            if (!v->active) continue;
            /* portamento */
            if (porta > 0.f) v->pitch = approach(v->pitch, v->pitchTarget, porta * porta * 2.f / 2.3f + 1e-4f, m);
            else v->pitch = v->pitchTarget;
            pitch = v->pitch + bendSemis + g->beamSp[BEAM_BEND] * 12.f;
            for (t = 0; t < NTONES; t++) {
                ToneSt *s = &v->t[t];
                float lf, ml, semis, fhz, oct, fvs, avs, amp, pan, gl, gr;
                on[t] = EP(t, ON) > 0.5f;
                wave[t] = (int)EP(t, WAVE);
                var[t] = (int)EP(t, VAR);
                fm[t] = (int)EP(t, FMODE);
                sl[t] = EP(t, SLOPE) > 0.5f;
                if (!on[t] && !(t == 0 && mod)) { s->gl1 = s->gr1 = 0.f; continue; }
                /* envelopes, LFOs */
                env_adsr(&s->aenv, EP(t, AA), EP(t, AD), EP(t, AS), EP(t, AR), m);
                env_adsr(&s->fenv, EP(t, FA), EP(t, FD), EP(t, FS), EP(t, FR), m);
                env_ad(&s->penv, EP(t, PA), EP(t, PD), m);
                {
                    const float fsec = EP(t, LFADE) / 127.f;
                    s->fade = fsec <= 0.f ? 1.f : clampf(s->fade + (float)m / (fsec * fsec * 6.f * SR), 0, 1);
                }
                lf = lfo_tick(&s->lfo, (int)EP(t, LSHAPE), lfo_hz(g, (int)EP(t, LRATE), EP(t, LSYNC) > 0.5f, (int)EP(t, LNOTE)), m) * s->fade;
                ml = lfo_tick(&s->mlfo, (int)EP(t, MSHAPE), lfo_hz(g, (int)EP(t, MRATE), EP(t, MSYNC) > 0.5f, (int)EP(t, MNOTE)), m) * modAmt;
                /* pitch */
                semis = pitch + EP(t, PITCH) + EP(t, DETUNE) * 0.01f + curve(EP(t, PDEPTH), 2.f) * 36.f * s->penv.lvl
                      + (curve(EP(t, LPITCH), 2.f) * lf + curve(EP(t, MPITCH), 2.f) * ml) * 12.f;
                s->inc = clampf(440.f * exp2f((semis - 69.f) * (1.f / 12.f)) / SR, 0.f, 0.45f);
                pwv[t] = clampf(EP(t, PW) / 127.f + EP(t, PWM) / 127.f * 0.5f * lf, 0.f, 1.f);
                /* filter */
                fvs = EP(t, FVELO) / 63.f;
                fvs = fvs >= 0 ? 1.f - fvs * (1.f - v->vel) : 1.f + fvs * v->vel;
                oct = EP(t, KF) / 100.f * (v->pitch - 60.f) / 12.f
                    + curve(EP(t, FDEPTH), 1.5f) * 8.f * s->fenv.lvl * fvs
                    + (curve(EP(t, LFILTER), 1.5f) * lf + curve(EP(t, MFILTER), 1.5f) * ml) * 4.f;
                fhz = clampf(cutoff_hz(EP(t, CUTOFF)) * exp2f(oct), 16.f, SR * 0.45f);
                gk[t] = tanf(PI_F * fhz / SR);
                {
                    const float r = EP(t, RESO) / 127.f;
                    kk[t] = 2.f - 1.97f * powf(r, 0.8f);
                    if (fm[t] == F_PKG) {   /* boost 2x (+6 dB) .. 15.8x (+24 dB), Q 0.8 .. 12 */
                        s->gainPk = exp2f((6.f + 18.f * r) / 6.0206f) - 1.f;
                        kk[t] = 1.f / (0.8f + 11.2f * r * sqrtf(r));
                    }
                }
                /* amp */
                avs = EP(t, AVELO) / 63.f;
                avs = avs >= 0 ? 1.f - avs * (1.f - v->vel) : 1.f + avs * v->vel;
                amp = EP(t, LEVEL) / 127.f;
                amp = amp * amp * s->aenv.lvl * avs;
                amp *= clampf(1.f + 0.5f * (curve(EP(t, LAMP), 1.f) * lf + curve(EP(t, MAMP), 1.f) * ml), 0.f, 2.f);
                pan = clampf(EP(t, PAN) / 64.f + curve(EP(t, LPAN), 1.f) * lf + curve(EP(t, MPAN), 1.f) * ml, -1.f, 1.f);
                gl = amp * cosf((pan + 1.f) * PI_F * 0.25f);
                gr = amp * sinf((pan + 1.f) * PI_F * 0.25f);
                if (!on[t]) gl = gr = 0.f;
                s->gl0 = s->gl1; s->gr0 = s->gr1;
                s->gl1 = gl; s->gr1 = gr;
                dgl[t] = (gl - s->gl0) / (float)m;
                dgr[t] = (gr - s->gr0) / (float)m;
                if (s->aenv.stage) alive = 1;
            }
            if (!alive) { v->active = 0; continue; }
            /* the samples */
            {
                float gl[NTONES], gr[NTONES];
                for (t = 0; t < NTONES; t++) { gl[t] = v->t[t].gl0; gr[t] = v->t[t].gr0; }
                for (i = 0; i < m; i++) {
                    float sl_ = 0.f, sr_ = 0.f, o0 = 0.f;
                    for (t = 0; t < NTONES; t++) {
                        ToneSt *s = &v->t[t];
                        float o;
                        if (!on[t] && !(t == 0 && mod)) continue;
                        if (t == 1 && mod == 1 && v->t[0].wrapped) osc_sync(s);
                        o = osc_tick(s, wave[t], var[t], pwv[t], v->pitch);
                        if (t == 0) o0 = o;
                        if (t == 1 && mod == 2) o *= o0 * 1.4f;
                        if (!on[t]) continue;
                        o = filter(s, fm[t], sl[t], gk[t], kk[t], o);
                        gl[t] += dgl[t]; gr[t] += dgr[t];
                        sl_ += o * gl[t];
                        sr_ += o * gr[t];
                    }
                    L[off + i] += sl_;
                    R[off + i] += sr_;
                }
            }
        }
    }
}
