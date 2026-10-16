/* GAIAXXL -- the arpeggiator: GRID (4 / 8 / 16, straight, light and heavy shuffle, triplets), DURATION, the GAIA's
 * twelve motifs (UP, DOWN, UP&DOWN, RANDOM, each plain, with the lowest note as a pedal (/L) or lowest and highest
 * (/L&H), and PHRASE), OCTAVE range, ACCENT, VELOCITY (REAL or fixed), KEY HOLD.
 * On MPC's tempo; while MPC plays, the steps sit on its grid (the song position the host gives every block). */
#include <string.h>
#include "gaia.h"

static const float GRID_SPB[N_GRID] = {1, 2, 2, 2, 3, 4, 4, 4, 6};          /* steps per beat */
static const float GRID_SWING[N_GRID] = {0, 0, 0.1f, 0.33f, 0, 0, 0.1f, 0.33f, 0};
static const float DURS[10] = {0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.f, 1.2f, 1.6f};

static int held_sorted(const Gaia *g, int *out)
{
    const Arp *a = &g->arp;
    const int *src = a->n ? a->notes : a->latched;
    const int n = a->n ? a->n : (g->v[P_ARP_HOLD] > 0.5f ? a->nl : 0);
    int i, j, cnt = 0, oct = (int)g->v[P_ARP_OCT], o, base[MAX_HELD];
    for (i = 0; i < n; i++) base[i] = src[i];
    for (i = 1; i < n; i++)   /* insertion sort, ascending */
        for (j = i; j > 0 && base[j] < base[j - 1]; j--) { const int t = base[j]; base[j] = base[j - 1]; base[j - 1] = t; }
    for (o = 0; o <= (oct < 0 ? -oct : oct); o++)
        for (i = 0; i < n && cnt < 64; i++) {
            const int nn = base[i] + (oct < 0 ? -12 : 12) * o;
            if (nn >= 0 && nn < 128) out[cnt++] = nn;
        }
    if (oct < 0)   /* downward octaves: keep the list ascending */
        for (i = 1; i < cnt; i++)
            for (j = i; j > 0 && out[j] < out[j - 1]; j--) { const int t = out[j]; out[j] = out[j - 1]; out[j - 1] = t; }
    return cnt;
}

static int key_velocity(const Gaia *g)
{
    const Arp *a = &g->arp;
    return a->n ? a->vels[a->n - 1] : 100;
}

/* the note for step k */
static int pick(Gaia *g, int k, const int *p, int n)
{
    const int motif = (int)g->v[P_ARP_MOTIF];
    const int lo = p[0], hi = p[n - 1];
    int seq[64], m = 0, i;
    if (n == 1) return p[0];
    switch (motif) {
    case 0: case 3: case 6: case 9:     /* /L: the lowest note every other step */
        if (!(k & 1)) return lo;
        for (i = 1; i < n; i++) seq[m++] = p[i];
        k >>= 1;
        break;
    case 1: case 4: case 7:             /* /L&H: low, x, high, x */
        if (!(k & 1)) return (k & 2) ? hi : lo;
        for (i = 1; i < n - 1; i++) seq[m++] = p[i];
        if (!m) return (k & 2) ? lo : hi;
        k >>= 1;
        break;
    case 11: {                          /* PHRASE: a fixed figure over the chord */
        static const int8_t F[16] = {0, 2, 1, 2, 0, 3, 2, 1, 0, 2, 1, 3, 2, 1, 4, 2};
        const int f = F[k & 15];
        return p[f % n] + 12 * (f / n);
    }
    default:
        for (i = 0; i < n; i++) seq[m++] = p[i];
        break;
    }
    if (motif <= 2) return seq[k % m];                                   /* UP */
    if (motif <= 5) return seq[m - 1 - (k % m)];                         /* DOWN */
    if (motif <= 8) {                                                    /* UP&DOWN */
        const int per = m > 1 ? 2 * m - 2 : 1, q = k % per;
        return seq[q < m ? q : per - q];
    }
    return seq[xr(&g->arp.rnd) % (uint32_t)m];                           /* RANDOM */
}

void arp_reset(Gaia *g)
{
    Arp *a = &g->arp;
    if (a->playing >= 0) synth_key_off(g, a->playing);
    a->playing = -1;
    a->n = a->nl = 0;
    a->step = 0;
    a->pos = 0;
    a->nextStep = 0;
}

void arp_key(Gaia *g, int note, int vel, int on)
{
    Arp *a = &g->arp;
    int i;
    for (i = 0; i < a->n; i++)
        if (a->notes[i] == note) {
            memmove(a->notes + i, a->notes + i + 1, (size_t)(a->n - i - 1) * sizeof(int));
            memmove(a->vels + i, a->vels + i + 1, (size_t)(a->n - i - 1) * sizeof(int));
            a->n--;
            break;
        }
    if (on) {
        if (a->n == 0) {   /* a new chord: KEY HOLD forgets the old one, the pattern starts again */
            a->nl = 0;
            if (a->playing < 0) { a->step = 0; a->pos = 0; a->nextStep = 0; }
        }
        if (a->n < MAX_HELD) { a->notes[a->n] = note; a->vels[a->n] = vel; a->n++; }
        if (a->nl < MAX_HELD) a->latched[a->nl++] = note;
    }
}

void arp_tick(Gaia *g, int frames)
{
    Arp *a = &g->arp;
    int pool[64], n;
    const int grid = (int)g->v[P_ARP_GRID];
    const float spb = GRID_SPB[grid < 0 ? 0 : grid >= N_GRID ? N_GRID - 1 : grid];
    const float bpm = g->tempo > 20 ? (float)g->tempo : 120.f;
    if (g->v[P_ARP_ON] < 0.5f) {
        if (a->playing >= 0) { synth_key_off(g, a->playing); a->playing = -1; }
        return;
    }
    n = held_sorted(g, pool);
    if (!n) {
        if (a->playing >= 0) { synth_key_off(g, a->playing); a->playing = -1; }
        a->step = 0;
        return;
    }
    /* the position, in steps: MPC's song position while it plays, else free on its tempo */
    if (g->hostPlaying) {
        const double want = g->hostPpq * spb;
        if (a->pos < want - 0.5 || a->pos > want + 0.5) {   /* a jump (loop, locate): resync to the grid */
            a->pos = want;
            a->step = (int)(want + 1e-6);
            a->nextStep = (double)a->step;
            if (a->nextStep < want) { a->step++; a->nextStep = a->step; }
        }
    }
    a->pos += (double)frames * bpm / 60.0 / SR * spb;
    if (g->hostPlaying) g->hostPpq += (double)frames * bpm / 60.0 / SR;
    if (a->playing >= 0 && a->pos >= a->offAt) { synth_key_off(g, a->playing); a->playing = -1; }
    while (a->pos >= a->nextStep + (a->step & 1 ? GRID_SWING[grid] : 0.f)) {
        const int dur = (int)g->v[P_ARP_DUR];
        const float acc = g->v[P_ARP_ACCENT] / 100.f;
        int vel = g->v[P_ARP_VELO] > 0.5f ? (int)g->v[P_ARP_VELO] : key_velocity(g);
        const int note = pick(g, a->step, pool, n);
        if (a->playing >= 0) synth_key_off(g, a->playing);
        if ((a->step % (int)spb) != 0) vel = (int)((float)vel * (1.f - 0.45f * acc));   /* accent: the beats louder */
        synth_key_on(g, note, vel < 1 ? 1 : vel > 127 ? 127 : vel);
        a->playing = note;
        a->offAt = (float)(a->nextStep + DURS[dur < 0 ? 0 : dur > 9 ? 9 : dur]);
        a->step++;
        a->nextStep = (double)a->step;
    }
}
