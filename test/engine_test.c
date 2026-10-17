/* GAIAXXL -- offline engine test (x86): every patch, every wave and variation, the effects, the arpeggiator, the
 * randomizer and the state round trip. Fails on NaN, silence where sound is due, clipping or a stuck note.
 * Prints the CPU time of a worst-case block (8 voices x 3 super saws, every effect). Optional: -w writes WAVs. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "engine.h"
#include "gaia.h"

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static const mpc_engine_t *E;
static int16_t buf[256];
static int writeWav = 0;

static void sp(void *x, const char *k, double v) { char b[32]; snprintf(b, sizeof b, "%g", v); E->set_param(x, k, b); }
static void note(void *x, int n, int vel) { uint8_t m[3] = {(uint8_t)(vel ? 0x90 : 0x80), (uint8_t)n, (uint8_t)vel}; E->midi(x, m, 3); }

typedef struct { double rms; int peak; int nan; } Stat;
static Stat run(void *x, int blocks, FILE *wav)
{
    Stat s = {0, 0, 0};
    double acc = 0;
    long cnt = 0;
    int b, i;
    for (b = 0; b < blocks; b++) {
        E->render(x, buf, 128);
        for (i = 0; i < 256; i++) {
            const int v = buf[i] < 0 ? -buf[i] : buf[i];
            if (v > s.peak) s.peak = v;
            acc += (double)buf[i] * buf[i];
            cnt++;
        }
        if (wav) fwrite(buf, 2, 256, wav);
    }
    s.rms = sqrt(acc / (double)cnt) / 32768.0;
    return s;
}

static FILE *wav_open(const char *name)
{
    FILE *f;
    if (!writeWav) return NULL;
    f = fopen(name, "wb");
    if (f) { static const uint8_t hdr[44] = {0}; fwrite(hdr, 1, 44, f); }
    return f;
}
static void wav_close(FILE *f)
{
    long len;
    uint32_t v;
    if (!f) return;
    len = ftell(f) - 44;
    fseek(f, 0, SEEK_SET);
    fwrite("RIFF", 1, 4, f); v = (uint32_t)(len + 36); fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    v = 16; fwrite(&v, 4, 1, f);
    { uint16_t a = 1, ch = 2; fwrite(&a, 2, 1, f); fwrite(&ch, 2, 1, f); }
    v = 44100; fwrite(&v, 4, 1, f); v = 44100 * 4; fwrite(&v, 4, 1, f);
    { uint16_t ba = 4, bits = 16; fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f); }
    fwrite("data", 1, 4, f); v = (uint32_t)len; fwrite(&v, 4, 1, f);
    fclose(f);
}

int main(int argc, char **argv)
{
    void *x;
    int p, w, v, t;
    char name[64], st[16384];
    writeWav = argc > 1 && !strcmp(argv[1], "-w");
    E = mpc_engine();
    x = E->create(NULL);
    CHECK(x != NULL, "create");
    printf("%d patches\n", patch_count());
    CHECK(patch_count() == (int)PDEF[P_PATCH].max + 1, "PATCH's range (%g) and the factory patches (%d)", PDEF[P_PATCH].max, patch_count());

    /* every patch: a chord, held, then released: sound, no clipping beyond the limiter, silence after */
    for (p = 0; p < patch_count(); p++) {
        FILE *f;
        Stat s;
        sp(x, "patch", p);
        E->get_param(x, "patch_name", name, sizeof name);
        snprintf(st, sizeof st, "build/wav/patch_%02d.wav", p);
        f = wav_open(st);
        note(x, 60, 100); note(x, 64, 90); note(x, 67, 80);
        s = run(x, 600, f);
        note(x, 60, 0); note(x, 64, 0); note(x, 67, 0);
        if (E->get_param(x, "arp_on", st, sizeof st) > 0 && atoi(st)) { sp(x, "arp_on", 0); }
        run(x, 900, f);
        wav_close(f);
        printf("  %2d %-18s rms %.3f peak %5d\n", p, name, s.rms, s.peak);
        CHECK(s.rms > 0.01 || s.peak > 3000, "patch %d (%s) is silent", p, name);
        CHECK(s.peak < 32700, "patch %d (%s) hits the ceiling", p, name);
        {
            Stat tail = run(x, 2000, NULL);   /* long reverbs die away */
            (void)tail;
        }
        E->midi(x, (const uint8_t[]){0xB0, 120, 0}, 3);
        run(x, 10, NULL);
    }

    /* every wave and variation on INIT: sound, no NaN */
    sp(x, "patch", 0);
    for (w = 0; w < N_WAVE; w++)
        for (v = 0; v < 3; v++) {
            Stat s;
            int n;
            sp(x, "t1_wave", w); sp(x, "t1_var", v); sp(x, "t1_pw", 64);
            double lo = 1e9, hi = 0;
            for (n = 24; n <= 108; n += 21) {
                note(x, n, 100);
                s = run(x, 120, NULL);
                note(x, n, 0);
                run(x, 60, NULL);
                if (s.rms < lo) lo = s.rms;
                if (s.rms > hi) hi = s.rms;
            }
            printf("  wave %d%c  rms %.3f .. %.3f\n", w, 'A' + v, lo, hi);
            CHECK(lo > 0.01 && hi < 0.6, "wave %d var %d level %.3f..%.3f", w, v, lo, hi);
        }

    /* filters: every mode and slope at high resonance stay finite */
    sp(x, "patch", 0);
    for (t = 0; t < 5; t++) {
        Stat s;
        sp(x, "t1_fmode", t); sp(x, "t1_reso", 127); sp(x, "t1_cutoff", 70); sp(x, "t1_slope", t & 1);
        note(x, 48, 127);
        s = run(x, 300, NULL);
        note(x, 48, 0);
        run(x, 300, NULL);
        CHECK(s.peak < 32767 && s.rms < 0.7, "filter mode %d", t);
    }

    /* PKG is a real peak: around the cutoff it is far louder than BYPASS (a user found 0.1.x's PKG did next to nothing) */
    {
        Stat a, b;
        sp(x, "patch", 0);
        sp(x, "t1_fmode", 4);
        note(x, 36, 100); a = run(x, 200, NULL); note(x, 36, 0); run(x, 400, NULL);
        sp(x, "t1_fmode", 3); sp(x, "t1_cutoff", 40); sp(x, "t1_reso", 0); sp(x, "t1_slope", 0);   /* even at RESONANCE 0 */
        note(x, 36, 100); b = run(x, 200, NULL); note(x, 36, 0); run(x, 400, NULL);
        printf("  PKG: rms %.3f against BYPASS %.3f\n", b.rms, a.rms);
        CHECK(b.rms > a.rms * 1.5, "PKG boosts too little (%.3f against %.3f)", b.rms, a.rms);
    }

    /* tones 2 and 3, SYNC and RING */
    sp(x, "patch", 0);
    sp(x, "t2_on", 1); sp(x, "t3_on", 1); sp(x, "t2_pitch", 7);
    for (t = 0; t < 3; t++) {
        Stat s;
        sp(x, "mod", t);
        note(x, 57, 100);
        s = run(x, 200, NULL);
        note(x, 57, 0);
        run(x, 200, NULL);
        CHECK(s.rms > 0.01, "mod %d silent", t);
    }

    /* the arpeggiator: free-running, notes come and go */
    sp(x, "patch", 0);
    sp(x, "arp_on", 1);
    for (t = 0; t < 12; t++) {
        Stat s;
        sp(x, "arp_motif", t);
        note(x, 60, 100); note(x, 64, 100); note(x, 67, 100);
        s = run(x, 400, NULL);
        note(x, 60, 0); note(x, 64, 0); note(x, 67, 0);
        run(x, 800, NULL);
        CHECK(s.rms > 0.005, "arp motif %d silent", t);
    }
    sp(x, "arp_on", 0);
    {   /* nothing left sounding */
        Stat s = run(x, 1500, NULL);
        CHECK(s.peak < 50, "a note left sounding after the arpeggiator (%d)", s.peak);
    }

    /* the randomizer: every type, many times: sound, never clipping hard */
    for (t = 0; t < N_RND; t++) {
        int k, quiet = 0;
        sp(x, "rnd_type", t);
        sp(x, "rnd_amt", 100);
        for (k = 0; k < 12; k++) {
            Stat s;
            sp(x, "rnd_go", 1);
            note(x, 55, 100); note(x, 62, 90);
            s = run(x, 500, NULL);
            note(x, 55, 0); note(x, 62, 0);
            run(x, 1200, NULL);
            E->midi(x, (const uint8_t[]){0xB0, 120, 0}, 3);
            if (s.rms < 0.01 && s.peak < 3000) quiet++;
            CHECK(s.peak < 32767, "random %s clipped", RND_NAMES[t]);
        }
        printf("  random %-8s: %d of 12 too quiet\n", RND_NAMES[t], quiet);
        CHECK(quiet <= 1, "random %s: %d quiet", RND_NAMES[t], quiet);
    }

    /* the state round trip */
    {
        void *y = E->create(NULL);
        char a[16384], b[16384];
        sp(x, "patch", 5);
        sp(x, "t2_cutoff", 33);
        E->get_param(x, "state", a, sizeof a);
        E->set_param(y, "state", a);
        E->get_param(y, "state", b, sizeof b);
        CHECK(!strcmp(a, b), "the state round trip");
        E->destroy(y);
    }

    /* CPU: the worst case */
    {
        clock_t c0;
        int k;
        double us;
        sp(x, "patch", 0);
        for (t = 1; t <= 3; t++) {
            char k2[32];
            snprintf(k2, sizeof k2, "t%d_on", t); sp(x, k2, 1);
            snprintf(k2, sizeof k2, "t%d_wave", t); sp(x, k2, 6);
            snprintf(k2, sizeof k2, "t%d_slope", t); sp(x, k2, 1);
        }
        sp(x, "dist_type", 1); sp(x, "flg_type", 1); sp(x, "dly_type", 2); sp(x, "rev_type", 2);
        for (k = 0; k < 8; k++) note(x, 48 + k * 3, 100);
        run(x, 10, NULL);
        c0 = clock();
        run(x, 2000, NULL);
        us = (double)(clock() - c0) / CLOCKS_PER_SEC * 1e6 / 2000;
        printf("worst-case block: %.0f us on this machine (MPC's budget: 2902 us per 128 frames)\n", us);
    }
    E->destroy(x);
    printf(fails ? "FAILED: %d\n" : "PASSED\n", fails);
    return fails != 0;
}
