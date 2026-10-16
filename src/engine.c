/* GAIAXXL -- the plugin engine (mpc-vst-plugins wrapper/engine.h): parameters, MIDI and audio for one instance. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine.h"
#include "gaia.h"

typedef struct { Gaia g; } Inst;

static void *create(const char *dataDir)
{
    Inst *in = calloc(1, sizeof *in);
    (void)dataDir;
    if (!in) return NULL;
    in->g.fx = fx_new();
    if (!in->g.fx) { free(in); return NULL; }
    osc_init();
    gaia_init(&in->g);
    in->g.v[P_RND_TYPE] = 0;
    in->g.v[P_RND_AMT] = 100;
    in->g.v[P_PATCH] = 1;
    patch_load(&in->g, 1);
    return in;
}

static void destroy(void *p)
{
    Inst *in = p;
    fx_free(in->g.fx);
    free(in);
}

static void midi(void *p, const uint8_t *m, int len)
{
    Gaia *g = &((Inst *)p)->g;
    const uint8_t st = m[0] & 0xF0;
    if (len < 2) return;
    if (st == 0x90 && len >= 3 && m[2]) gaia_note_on(g, m[1] & 127, m[2] & 127);
    else if (st == 0x80 || (st == 0x90 && len >= 3)) gaia_note_off(g, m[1] & 127);
    else if (st == 0xE0 && len >= 3) {
        const int v = ((m[2] & 127) << 7 | (m[1] & 127)) - 8192;
        g->bend = (float)v / (v >= 0 ? 8191.f : 8192.f);
    } else if (st == 0xB0 && len >= 3) {
        const int cc = m[1], v = m[2] & 127;
        if (cc == 1) g->modWheel = (float)v / 127.f;
        else if (cc == 64) {
            const int was = g->sustain;
            g->sustain = v >= 64;
            if (was && !g->sustain) {   /* the pedal up: release what no key holds */
                int i;
                for (i = 0; i < NVOICES; i++)
                    if (g->voice[i].active && g->voice[i].gate && g->v[P_MONO] < 0.5f) {
                        int held = 0, k;
                        for (k = 0; k < g->arp.n; k++) if (g->arp.notes[k] == g->voice[i].note) held = 1;
                        if (!held && g->v[P_ARP_ON] < 0.5f) synth_key_off(g, g->voice[i].note - (int)g->v[P_OCTAVE] * 12);
                    }
            }
        } else if (cc == 120 || cc == 123) gaia_all_off(g, cc == 120);
    }
}

static int key_index(const char *k)
{
    int i;
    for (i = 0; i < P_COUNT; i++) if (!strcmp(k, PDEF[i].key)) return i;
    return -1;
}

/* ---------------------------------------------------------------- the state: "key=value" lines */
static char *serialize(Gaia *g, int *len)
{
    const size_t cap = 64 + P_COUNT * 40;
    char *s = malloc(cap);
    size_t n = 0;
    int k;
    if (!s) return NULL;
    n += (size_t)snprintf(s + n, cap - n, "GAIAXXL 1\nname=%s\nfrom=%d\ndirty=%d\n", g->patchName, g->patch, g->dirty);
    for (k = 0; k < P_COUNT; k++)
        if (PDEF[k].kind == PK_PATCH || PDEF[k].kind == PK_UI)
            n += (size_t)snprintf(s + n, cap - n, "%s=%g\n", PDEF[k].key, g->v[k]);
    *len = (int)n;
    return s;
}

static void deserialize(Gaia *g, const char *s)
{
    const char *p = s;
    if (strncmp(s, "GAIAXXL ", 8)) return;
    gaia_all_off(g, 1);
    patch_init(g);
    while ((p = strchr(p, '\n'))) {
        char key[40];
        int n = 0, k;
        p++;
        while (*p && *p != '=' && *p != '\n' && n < 39) key[n++] = *p++;
        key[n] = 0;
        if (*p != '=') continue;
        p++;
        if (!strcmp(key, "name")) {
            int j = 0;
            while (*p && *p != '\n' && j < (int)sizeof g->patchName - 1) g->patchName[j++] = *p++;
            g->patchName[j] = 0;
            continue;
        }
        if (!strcmp(key, "from")) { g->patch = atoi(p); continue; }
        if (!strcmp(key, "dirty")) { g->dirty = atoi(p); continue; }
        k = key_index(key);
        if (k >= 0) g->v[k] = clampf((float)atof(p), PDEF[k].min, PDEF[k].max);
    }
    g->rev++;
}

static void set_param(void *p, const char *k, const char *val)
{
    Gaia *g = &((Inst *)p)->g;
    int i;
    if (!strcmp(k, "state")) { deserialize(g, val); return; }
    if (!strcmp(k, "host_time")) {
        int playing = 0;
        double ppq = 0, tempo = 0;
        if (sscanf(val, "%d %lf %lf", &playing, &ppq, &tempo) == 3) {
            if (tempo > 20) g->tempo = tempo;
            g->hostPlaying = playing;
            g->hostPpq = ppq;
        }
        return;
    }
    i = key_index(k);
    if (i < 0) return;
    {
        const ParamDef *d = &PDEF[i];
        const float f = clampf((float)atof(val), d->min, d->max);
        switch (d->kind) {
        case PK_PATCH:
            if (g->v[i] != f) { g->v[i] = f; g->dirty = 1; }
            if (i == P_ARP_ON && f < 0.5f) arp_reset(g);
            if (i == P_MONO || i == P_OCTAVE) gaia_all_off(g, 0);
            break;
        case PK_UI:
            if (i == P_PATCH) {
                const int n = (int)(f + 0.5f);
                g->v[i] = (float)n;
                if (n != g->patch || g->dirty) { gaia_all_off(g, 0); patch_load(g, n); }
            } else g->v[i] = f;
            break;
        case PK_TRIG:
            if (i == P_RND_GO && f > 0.5f) random_patch(g, (int)g->v[P_RND_TYPE], g->v[P_RND_AMT] / 100.f);
            break;
        default: break;
        }
    }
    g->rev++;
}

static int put(char *buf, int n, const char *s)
{
    if (n <= 0) return 0;
    snprintf(buf, (size_t)n, "%s", s);
    return (int)strlen(s) > 0 ? (int)strlen(s) : 1;
}

static int get_param(void *p, const char *k, char *buf, int n)
{
    Gaia *g = &((Inst *)p)->g;
    char t[64];
    int i;
    if (!strcmp(k, "state")) {
        int len = 0, r = 0;
        char *s = serialize(g, &len);
        if (s) {
            if (len < n) memcpy(buf, s, (size_t)len + 1);
            r = len + 1;
            free(s);
        }
        return r;
    }
    if (!strncmp(k, "patch:", 6)) return put(buf, n, patch_name(atoi(k + 6)));
    if (!strcmp(k, "patch_name")) {
        snprintf(t, sizeof t, "%s%s", g->patchName, g->dirty ? " *" : "");
        return put(buf, n, t);
    }
    i = key_index(k);
    if (i < 0) return 0;
    if (PDEF[i].kind == PK_TRIG) return put(buf, n, "0");
    if (PDEF[i].nopts || PDEF[i].kind != PK_SHOW) {
        if (PDEF[i].nopts) snprintf(t, sizeof t, "%d", (int)g->v[i]);
        else snprintf(t, sizeof t, "%g", g->v[i]);
        return put(buf, n, t);
    }
    return 0;
}

/* Denormals: the audio runs with flush-to-zero on, the host's setting put back afterwards */
#if defined(__arm__)
static inline unsigned fz_on(void)
{
    unsigned old;
    __asm__ volatile("vmrs %0, fpscr" : "=r"(old));
    __asm__ volatile("vmsr fpscr, %0" : : "r"(old | (1u << 24)));
    return old;
}
static inline void fz_off(unsigned old) { __asm__ volatile("vmsr fpscr, %0" : : "r"(old)); }
#elif defined(__x86_64__) || defined(__i386__)
#include <xmmintrin.h>
static inline unsigned fz_on(void) { const unsigned old = _mm_getcsr(); _mm_setcsr(old | 0x8040u); return old; }
static inline void fz_off(unsigned old) { _mm_setcsr(old); }
#else
static inline unsigned fz_on(void) { return 0; }
static inline void fz_off(unsigned old) { (void)old; }
#endif

static void render(void *p, int16_t *out, int frames)
{
    const unsigned fpu = fz_on();
    Gaia *g = &((Inst *)p)->g;
    float L[128], R[128];
    int i;
    float lvl;
    if (frames > 128) frames = 128;
    gaia_render(g, L, R, frames);
    fx_process(g->fx, g, L, R, frames);
    lvl = g->v[P_LEVEL] / 127.f;
    lvl = lvl * lvl * 0.42f;
    for (i = 0; i < frames; i++) {
        float l = L[i] * lvl, r = R[i] * lvl;
        /* a soft limit above -3 dB */
        l = l > 0.7f ? 0.7f + 0.3f * tanhf((l - 0.7f) / 0.3f) : l < -0.7f ? -0.7f + 0.3f * tanhf((l + 0.7f) / 0.3f) : l;
        r = r > 0.7f ? 0.7f + 0.3f * tanhf((r - 0.7f) / 0.3f) : r < -0.7f ? -0.7f + 0.3f * tanhf((r + 0.7f) / 0.3f) : r;
        out[2 * i] = (int16_t)(l * 32767.f);
        out[2 * i + 1] = (int16_t)(r * 32767.f);
    }
    fz_off(fpu);
}

static const mpc_engine_t ENGINE = {create, destroy, midi, set_param, get_param, render, NULL};
const mpc_engine_t *mpc_engine(void) { return &ENGINE; }
