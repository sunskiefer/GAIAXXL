/* GAIAXXL -- a three-tone virtual-analog synth for Akai Force / MPC OS (VST2 through mpc-vst-plugins).
 * Copyright 2026 L'Cronx (sunskiefer). GPL-3.0-or-later, see LICENSE. Korg's wave tables in src/korg/ are BSD-3-Clause.
 *
 * The patch is one float per parameter (src/params_gen.h), in the GAIA SH-01's own units (0..127, -63..+63 ...).
 * Audio: 44.1 kHz, rendered in chunks of CTRL samples; modulation is computed once per chunk and ramped inside it. */
#pragma once
#include <stdint.h>
#include "params_gen.h"

#define SR 44100.0f
#define NVOICES 8
#define NTONES 3
#define CTRL 16                  /* samples per modulation step */
#define NSAW 7                   /* the super saw's saws */
#define MAX_HELD 16

enum { W_SAW, W_SQR, W_PWSQR, W_TRI, W_SINE, W_NOISE, W_SSAW };
enum { F_LPF, F_HPF, F_BPF, F_PKG, F_BYPASS };

typedef struct { float lvl; int stage; } Env;   /* stage 0 off, 1 attack, 2 decay/sustain, 3 release */

typedef struct {
    float ph;                    /* 0..1 */
    float sh;                    /* sample & hold value */
    float out;
    uint32_t rnd;
    float rndFrom, rndTo;        /* RND: a smoothed random walk */
} Lfo;

typedef struct {
    /* oscillator */
    float ph[NSAW], ph2;         /* phases (0..1); ph2: the sub / second oscillator */
    float inc;                   /* this chunk's phase increment */
    float last;                  /* the last oscillator sample (ring modulation) */
    int wrapped;                 /* the master phase wrapped this sample (hard sync) */
    float pink[6], red;          /* noise colour filters (Korg's pink and red) */
    uint32_t nseed;
    float triInt;                /* the triangle's integrator (variation A) */
    /* filter: two state-variable stages */
    float s1[2], s2[2];
    float g, k, gainPk;          /* this chunk's coefficients */
    float g0, k0;                /* the previous chunk's, ramped across the chunk */
    /* envelopes, LFOs */
    Env penv, fenv, aenv;
    Lfo lfo, mlfo;
    float fade;                  /* the LFO's fade-in, 0..1 */
    /* amp: per-chunk start and end gains for left and right */
    float gl0, gr0, gl1, gr1;
} ToneSt;

typedef struct {
    int active;                  /* sounding (an envelope still running) */
    int gate;                    /* key down (or held by the arpeggiator / hold) */
    int note;
    float vel;                   /* 0..1 */
    float pitch, pitchTarget;    /* MIDI note, with portamento */
    uint32_t age;
    ToneSt t[NTONES];
} Voice;

/* the effects' state (fx.c) */
typedef struct Fx Fx;

typedef struct {
    int notes[MAX_HELD], vels[MAX_HELD], n;   /* the keys held, in the order pressed */
    int latched[MAX_HELD], nl;                /* KEY HOLD: the chord kept */
    double nextStep;                          /* in steps since the start */
    double pos;                               /* the running position in steps */
    int step;
    int playing;                              /* the note now sounding (-1 none) */
    float offAt;                              /* when that note ends (steps) */
    uint32_t rnd;
    int lastNote, dir, idx;
} Arp;

typedef struct {
    float v[P_COUNT];            /* the patch (and the UI parameters) */
    Voice voice[NVOICES];
    uint32_t clock;
    float bend;                  /* -1..1 */
    float modWheel;              /* 0..1 */
    int sustain;                 /* the sustain pedal */
    double tempo;                /* BPM from the host */
    int hostPlaying;
    double hostPpq;
    int monoStack[MAX_HELD], nMono;
    float monoVel[MAX_HELD];
    Arp arp;
    Fx *fx;
    uint32_t rnd;
    char patchName[24];
    int patch;                   /* the factory patch last loaded */
    int dirty;                   /* edited since */
    uint32_t rev;                /* bumped on any change the screen shows */
    /* synth.c's working state */
    float E[NTONES][TONE_PARAMS];    /* this chunk's tone parameters (the patch plus the beams) */
    float beamSp[N_BEAM];            /* the beams on what is not a tone parameter */
    float gph[NTONES], gmph[NTONES]; /* the free-running LFOs' phases */
    float lastPitch;
} Gaia;

/* synth.c */
void gaia_init(Gaia *g);
void gaia_note_on(Gaia *g, int note, int vel);
void gaia_note_off(Gaia *g, int note);
void gaia_all_off(Gaia *g, int hard);
void gaia_render(Gaia *g, float *L, float *R, int n);
float lfo_hz(const Gaia *g, int rate, int sync, int note);

/* voice-level entry points the arpeggiator uses (synth.c) */
void synth_key_on(Gaia *g, int note, int vel);
void synth_key_off(Gaia *g, int note);

/* osc.c */
void osc_init(void);
float osc_tick(ToneSt *s, int wave, int var, float pw, float note);
void osc_reset(ToneSt *s, uint32_t seed);
void osc_sync(ToneSt *s);

/* fx.c */
Fx *fx_new(void);
void fx_free(Fx *f);
void fx_clear(Fx *f);
void fx_process(Fx *f, const Gaia *g, float *L, float *R, int n);

/* arp.c */
void arp_key(Gaia *g, int note, int vel, int on);
void arp_tick(Gaia *g, int frames);
void arp_reset(Gaia *g);

/* patches.c */
int patch_count(void);
const char *patch_name(int i);
void patch_load(Gaia *g, int i);
void patch_init(Gaia *g);

/* random.c */
void random_patch(Gaia *g, int type, float amount);
void random_seeded(Gaia *g, int type, uint32_t seed);

static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static inline uint32_t xr(uint32_t *s)
{
    uint32_t x = *s ? *s : 0x9E3779B9u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return *s = x;
}
static inline float xf(uint32_t *s) { return (float)(xr(s) >> 8) * (1.f / 16777216.f); }   /* 0..1 */

#define V(g, p) ((g)->v[p])
#define TV(g, t, name) ((g)->v[TP(t, name)])
