/* GAIAXXL -- the factory patches (MPC's PRESET menu, the PATCH arrows). Each is the INIT patch plus the values listed;
 * names and sounds are GAIAXXL's own. After the hand-made ones come generated ones (the randomizer, fixed seeds). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gaia.h"

typedef struct { const char *name; const char *set; } Factory;

/* tone keys are "t1_cutoff" etc.; see tools/gen_params.py for every key */
static const Factory HAND[] = {
    {"INIT", ""},
    {"Supersaw Lead",
     "t1_wave=6 t1_pw=70 t1_cutoff=96 t1_reso=20 t1_fdepth=12 t1_fd=60 t1_fs=90 t1_aa=2 t1_ar=30 t1_lpitch=3 t1_lfade=60 "
     "t2_on=1 t2_wave=6 t2_var=1 t2_pitch=12 t2_level=70 t2_cutoff=90 t2_ar=30 mono=1 porta=1 ptime=18 "
     "dly_type=2 dly_note=14 dly_level=40 dly_fb=45 rev_type=2 rev_level=40"},
    {"Trance Pad",
     "t1_wave=6 t1_pw=95 t1_pwm=40 t1_cutoff=70 t1_fa=70 t1_fd=90 t1_fs=60 t1_fdepth=18 t1_aa=70 t1_ad=90 t1_as=120 t1_ar=80 "
     "t2_on=1 t2_wave=6 t2_var=2 t2_pitch=-12 t2_level=80 t2_cutoff=60 t2_aa=72 t2_ar=82 t2_pan=-20 "
     "t3_on=1 t3_wave=2 t3_var=2 t3_pitch=12 t3_pw=60 t3_pwm=50 t3_level=60 t3_aa=75 t3_ar=85 t3_pan=20 "
     "flg_type=1 flg_rate=20 flg_depth=60 flg_level=50 rev_type=2 rev_time=105 rev_level=65"},
    {"Acid Squelch",
     "t1_wave=0 t1_fmode=0 t1_slope=1 t1_cutoff=46 t1_reso=96 t1_fdepth=40 t1_fd=48 t1_fs=0 t1_fr=20 t1_fvelo=30 t1_aa=0 t1_ad=60 "
     "t1_as=100 t1_ar=12 t1_avelo=25 mono=1 porta=1 ptime=12 dist_type=1 dist_drive=50 dist_tone=70 dly_type=1 dly_note=15 dly_level=25"},
    {"Sync Scream",
     "t1_wave=0 t1_level=0 t2_on=1 t2_wave=0 t2_pitch=19 t2_pdepth=30 t2_pd=60 t2_cutoff=100 t2_reso=15 t2_fdepth=10 mod=1 "
     "mono=1 porta=1 ptime=10 dist_type=1 dist_drive=40 dly_type=2 dly_level=35 rev_type=1 rev_level=30"},
    {"PWM Strings",
     "t1_wave=2 t1_pw=40 t1_pwm=70 t1_lrate=70 t1_cutoff=88 t1_fdepth=6 t1_aa=58 t1_ad=80 t1_as=118 t1_ar=70 "
     "t2_on=1 t2_wave=2 t2_var=2 t2_pw=60 t2_pwm=60 t2_detune=9 t2_cutoff=84 t2_aa=60 t2_as=118 t2_ar=72 t2_pan=-30 "
     "t1_pan=30 flg_type=1 flg_rate=14 flg_depth=55 flg_level=45 rev_type=2 rev_time=96 rev_level=55"},
    {"Poly Brass",
     "t1_wave=0 t1_cutoff=46 t1_fa=30 t1_fd=62 t1_fs=70 t1_fdepth=34 t1_aa=24 t1_ad=60 t1_as=105 t1_ar=36 "
     "t2_on=1 t2_wave=0 t2_var=1 t2_detune=8 t2_cutoff=46 t2_fa=30 t2_fdepth=34 t2_aa=24 t2_as=105 t2_ar=36 "
     "rev_type=2 rev_level=35"},
    {"Fat Saw Bass",
     "t1_wave=0 t1_pitch=-12 t1_slope=1 t1_cutoff=40 t1_reso=30 t1_fdepth=30 t1_fd=55 t1_fs=20 t1_aa=0 t1_ad=70 t1_as=110 t1_ar=14 "
     "t2_on=1 t2_wave=1 t2_var=2 t2_pitch=-12 t2_detune=-6 t2_level=80 t2_slope=1 t2_cutoff=38 t2_fdepth=28 t2_ar=14 "
     "mono=1 boost=1"},
    {"Sub Pulse",
     "t1_wave=2 t1_var=1 t1_pw=30 t1_pitch=-12 t1_cutoff=46 t1_fdepth=22 t1_fd=40 t1_fs=10 t1_ad=60 t1_ar=10 mono=1 boost=1"},
    {"Glass Bell",
     "t1_wave=4 t1_var=2 t1_pw=45 t1_cutoff=127 t1_aa=0 t1_ad=82 t1_as=0 t1_ar=78 "
     "t2_on=1 t2_wave=4 t2_var=1 t2_pw=70 t2_pitch=24 t2_level=55 t2_ad=60 t2_as=0 t2_ar=60 "
     "dly_type=2 dly_level=35 rev_type=3 rev_time=95 rev_level=55"},
    {"Drawbar Organ",
     "t1_wave=4 t1_var=1 t1_pw=10 t1_aa=0 t1_ad=40 t1_as=127 t1_ar=6 t2_on=1 t2_wave=4 t2_var=1 t2_pw=55 t2_pitch=12 t2_level=70 "
     "t2_as=127 t2_ar=6 t3_on=1 t3_wave=4 t3_pitch=19 t3_level=50 t3_as=127 t3_ar=6 flg_type=2 flg_rate=50 flg_depth=40 "
     "flg_level=50 rev_type=1 rev_level=30"},
    {"Electric Keys",
     "t1_wave=3 t1_var=1 t1_pw=40 t1_cutoff=80 t1_fdepth=16 t1_fd=60 t1_fs=40 t1_fvelo=30 t1_ad=86 t1_as=30 t1_ar=50 t1_avelo=30 "
     "t2_on=1 t2_wave=4 t2_var=2 t2_pw=20 t2_pitch=12 t2_level=45 t2_ad=55 t2_as=0 t2_ar=40 t2_avelo=40 "
     "flg_type=2 flg_rate=25 flg_depth=50 flg_level=40 rev_type=1 rev_level=30"},
    {"Hollow Flute",
     "t1_wave=3 t1_cutoff=90 t1_aa=22 t1_as=110 t1_ar=30 t1_lpitch=3 t1_lfade=60 t2_on=1 t2_wave=5 t2_var=1 t2_fmode=2 "
     "t2_cutoff=96 t2_reso=40 t2_kf=100 t2_level=30 t2_aa=10 t2_ad=40 t2_as=40 t2_ar=20 rev_type=2 rev_level=45"},
    {"Square Lead",
     "t1_wave=1 t1_var=0 t1_cutoff=86 t1_reso=25 t1_fdepth=10 t1_aa=0 t1_ar=24 t1_lpitch=4 t1_lfade=50 mono=1 porta=1 ptime=14 "
     "dly_type=1 dly_note=12 dly_level=30 rev_type=1 rev_level=30"},
    {"Ring Bells",
     "t1_wave=4 t1_level=0 t2_on=1 t2_wave=4 t2_pitch=17 t2_ad=70 t2_as=0 t2_ar=70 mod=2 "
     "t3_on=1 t3_wave=4 t3_var=2 t3_pw=30 t3_level=60 t3_ad=80 t3_as=0 t3_ar=80 rev_type=3 rev_time=100 rev_level=60"},
    {"Noise Sweep",
     "t1_wave=5 t1_var=0 t1_fmode=2 t1_cutoff=30 t1_reso=80 t1_fa=90 t1_fd=100 t1_fs=0 t1_fdepth=50 t1_aa=80 t1_ad=100 t1_as=0 "
     "t1_ar=90 t1_lfilter=20 t1_lrate=40 flg_type=1 flg_level=60 rev_type=2 rev_time=110 rev_level=70"},
    {"Wobble Bass",
     "t1_wave=0 t1_pitch=-12 t1_slope=1 t1_cutoff=40 t1_reso=60 t1_lfilter=50 t1_lsync=1 t1_lnote=14 t1_lshape=0 t1_lkey=1 "
     "t2_on=1 t2_wave=1 t2_pitch=-24 t2_level=70 t2_cutoff=50 mono=1 dist_type=1 dist_drive=30 boost=1"},
    {"Analog Strings",
     "t1_wave=0 t1_cutoff=84 t1_aa=62 t1_ad=80 t1_as=120 t1_ar=75 t2_on=1 t2_wave=0 t2_var=1 t2_detune=12 t2_cutoff=80 t2_aa=64 "
     "t2_as=120 t2_ar=76 t2_pan=-25 t1_pan=25 flg_type=1 flg_rate=10 flg_depth=70 flg_fb=30 flg_level=55 rev_type=2 rev_time=100 rev_level=55"},
    {"Pluck Arp",
     "t1_wave=2 t1_pw=50 t1_cutoff=56 t1_reso=40 t1_fdepth=34 t1_fd=50 t1_fs=0 t1_ad=58 t1_as=0 t1_ar=40 t1_kf=50 "
     "arp_on=1 arp_grid=5 arp_motif=6 arp_oct=1 dly_type=2 dly_note=13 dly_level=40 dly_fb=40 rev_type=2 rev_level=35"},
    {"Fuzz Lead",
     "t1_wave=1 t1_var=2 t1_cutoff=100 t1_aa=0 t1_ar=24 mono=1 porta=1 ptime=8 dist_type=2 dist_drive=70 dist_tone=60 "
     "dly_type=1 dly_level=30 rev_type=1 rev_level=25"},
    {"Bit Crushed",
     "t1_wave=0 t1_cutoff=100 t1_ad=60 t1_as=60 t1_ar=30 dist_type=3 dist_drive=60 dist_tone=50 dist_level=85 dly_type=2 dly_level=30"},
    {"Phaser Chords",
     "t1_wave=6 t1_var=1 t1_pw=50 t1_cutoff=82 t1_ad=70 t1_as=80 t1_ar=40 flg_type=2 flg_rate=24 flg_depth=80 flg_fb=60 "
     "flg_level=70 rev_type=2 rev_level=40"},
    {"Octave Shifter",
     "t1_wave=0 t1_cutoff=90 t1_ar=30 flg_type=3 flg_pitch=12 flg_fb=20 flg_level=55 dly_type=1 dly_level=30 rev_type=2 rev_level=40"},
    {"Soft Sine Bass",
     "t1_wave=4 t1_pitch=-12 t1_ad=60 t1_as=110 t1_ar=14 t2_on=1 t2_wave=3 t2_level=40 t2_ar=14 mono=1 boost=1"},
    {"Saw Pluck",
     "t1_wave=0 t1_var=1 t1_cutoff=58 t1_reso=30 t1_fdepth=40 t1_fd=52 t1_fs=0 t1_ad=62 t1_as=0 t1_ar=40 "
     "t2_on=1 t2_wave=0 t2_detune=10 t2_cutoff=58 t2_fdepth=40 t2_fd=52 t2_fs=0 t2_ad=62 t2_as=0 t2_ar=40 rev_type=2 rev_level=40"},
    {"Hoover Rave",
     "t1_wave=6 t1_var=2 t1_pw=110 t1_pwm=80 t1_pdepth=-14 t1_pd=50 t1_lrate=88 t2_on=1 t2_wave=2 t2_var=2 t2_pitch=-12 t2_pw=90 "
     "t2_pwm=90 t2_level=80 mono=1 porta=1 ptime=30 flg_type=1 flg_level=50 rev_type=2 rev_level=40"},
    {"Choir Pad",
     "t1_wave=3 t1_var=2 t1_pw=60 t1_cutoff=76 t1_aa=70 t1_as=120 t1_ar=80 t1_lpitch=2 t2_on=1 t2_wave=5 t2_var=1 t2_fmode=2 "
     "t2_cutoff=82 t2_reso=60 t2_level=35 t2_aa=70 t2_ar=80 rev_type=2 rev_time=110 rev_level=65"},
    {"Laser Zap",
     "t1_wave=0 t1_pdepth=50 t1_pa=0 t1_pd=40 t1_cutoff=110 t1_ad=50 t1_as=0 t1_ar=20 dly_type=2 dly_level=45 dly_fb=55"},
    {"Mono Synth Lead",
     "t1_wave=0 t1_slope=1 t1_cutoff=58 t1_reso=45 t1_fdepth=26 t1_fd=58 t1_fs=50 t1_aa=0 t1_ar=24 t2_on=1 t2_wave=2 t2_pw=40 "
     "t2_detune=-8 t2_slope=1 t2_cutoff=58 t2_reso=45 t2_fdepth=26 t2_ar=24 mono=1 porta=1 ptime=16 dly_type=1 dly_level=30"},
    {"Shimmer Keys",
     "t1_wave=4 t1_var=1 t1_pw=80 t1_ad=80 t1_as=40 t1_ar=70 t2_on=1 t2_wave=3 t2_var=2 t2_pw=90 t2_pitch=12 t2_level=55 t2_ad=70 "
     "t2_as=20 t2_ar=70 flg_type=3 flg_pitch=12 flg_level=35 rev_type=3 rev_time=110 rev_level=60"},
    {"Gated Pad",
     "t1_wave=6 t1_cutoff=80 t1_aa=20 t1_as=120 t1_ar=60 t1_lamp=-55 t1_lshape=3 t1_lsync=1 t1_lnote=17 t1_lkey=1 rev_type=2 rev_level=40"},
    {"Deep Space",
     "t1_wave=6 t1_var=2 t1_pw=110 t1_cutoff=60 t1_lfilter=18 t1_lrate=30 t1_aa=95 t1_ar=100 t2_on=1 t2_wave=5 t2_var=2 t2_fmode=2 "
     "t2_cutoff=50 t2_reso=90 t2_lfilter=40 t2_lrate=20 t2_level=40 t2_aa=95 t2_ar=100 dly_type=2 dly_level=50 dly_fb=70 "
     "rev_type=2 rev_time=127 rev_level=90"},
};
#define N_HAND ((int)(sizeof HAND / sizeof HAND[0]))

/* the generated ones: a type (RANDOM TYPE's order) and a seed, named here */
typedef struct { const char *name; int type; uint32_t seed; } Gen;
static const Gen GEN[] = {
    {"Silk Pad", 0, 11}, {"Dawn Pad", 0, 23}, {"Cloud Pad", 0, 37}, {"Velvet Pad", 0, 41},
    {"Neon Lead", 1, 53}, {"Steel Lead", 1, 67}, {"Comet Lead", 1, 71}, {"Siren Lead", 1, 83},
    {"Rubber Bass", 2, 97}, {"Grit Bass", 2, 101}, {"Round Bass", 2, 113}, {"Pump Bass", 2, 127},
    {"Glass Pluck", 3, 131}, {"Kalimba Pluck", 3, 149}, {"Drip Pluck", 3, 151}, {"Harp Pluck", 3, 163},
    {"Tine Keys", 4, 173}, {"Felt Keys", 4, 181}, {"Clav Keys", 4, 191}, {"Toy Keys", 4, 197},
    {"Horn Section", 5, 211}, {"Soft Brass", 5, 223}, {"Bright Brass", 5, 227}, {"Swell Brass", 5, 229},
    {"Chamber Strings", 6, 233}, {"Wide Strings", 6, 239}, {"Slow Strings", 6, 241}, {"Velvet Strings", 6, 251},
    {"Radio Static", 7, 257}, {"Metal Rain", 7, 263}, {"Alien Talk", 7, 269}, {"Spinning Top", 7, 271},
};
#define N_GEN ((int)(sizeof GEN / sizeof GEN[0]))

int patch_count(void) { return N_HAND + N_GEN; }

const char *patch_name(int i)
{
    if (i < 0) i = 0;
    if (i < N_HAND) return HAND[i].name;
    if (i < N_HAND + N_GEN) return GEN[i - N_HAND].name;
    return "INIT";
}

void patch_init(Gaia *g)
{
    int k;
    for (k = 0; k < P_COUNT; k++)
        if (PDEF[k].kind == PK_PATCH) g->v[k] = PDEF[k].def;
}

static void apply(Gaia *g, const char *set)
{
    const char *p = set;
    while (*p) {
        char key[32];
        int n = 0, k;
        while (*p == ' ') p++;
        if (!*p) break;
        while (*p && *p != '=' && n < 31) key[n++] = *p++;
        key[n] = 0;
        if (*p != '=') break;
        p++;
        for (k = 0; k < P_COUNT; k++)
            if (!strcmp(PDEF[k].key, key)) { g->v[k] = clampf((float)atof(p), PDEF[k].min, PDEF[k].max); break; }
        while (*p && *p != ' ') p++;
    }
}

void patch_load(Gaia *g, int i)
{
    if (i < 0 || i >= patch_count()) i = 0;
    patch_init(g);
    if (i < N_HAND) apply(g, HAND[i].set);
    else random_seeded(g, GEN[i - N_HAND].type, GEN[i - N_HAND].seed * 2654435761u);
    snprintf(g->patchName, sizeof g->patchName, "%s", patch_name(i));
    g->patch = i;
    g->dirty = 0;
}
