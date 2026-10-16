#!/usr/bin/env python3
"""The single source of GAIAXXL's parameters: writes params.json (mpc-vst-plugins) and src/params_gen.h.

    python3 tools/gen_params.py          (writes both)
    python3 tools/gen_params.py --check  (fails when either is stale)

MPC saves a project's values by parameter index: once released, parameters are only ever appended to this list.

Values are in the units the GAIA SH-01's MIDI implementation gives (0..127, -63..+63, -24..+24 ...); the wrapper sends
whole numbers for "display": "int" parameters and the option's index for option lists, and the engine keeps them as
they are (src/gaia.h: the patch is one float per parameter).

Kinds (params_gen.h):
  PK_PATCH  stored in a patch (the sound)
  PK_UI     the patch selector, the randomizer's settings: in the plugin state, not in a patch
  PK_TRIG   a key (momentary): the randomizer's key, the patch and octave arrows
  PK_SHOW   read-only text (the patch's name)"""
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

WAVES = ["SAW", "SQR", "PW-SQR", "TRI", "SINE", "NOISE", "S-SAW"]
VARS = ["A", "B", "C"]
FMODES = ["LPF", "HPF", "BPF", "PKG", "BYPASS"]
SLOPES = ["-12dB", "-24dB"]
LFO_SHAPES = ["TRI", "SIN", "SAW", "SQR", "S&H", "RND"]
# the tempo-synced lengths (whole notes per cycle), the GAIA's list: 16 .. 1/32
SYNC_NOTES = ["16", "12", "8", "4", "2", "1", "3/4", "2/3", "1/2", "3/8", "1/3", "1/4", "3/16", "1/6", "1/8",
              "3/32", "1/12", "1/16", "1/24", "1/32"]
ONOFF = ["OFF", "ON"]
MODS = ["OFF", "SYNC", "RING"]
BEAM_DESTS = ["LFO RATE", "LFO FADE", "LFO PITCH", "LFO FILTER", "LFO AMP", "OSC PITCH", "OSC DETUNE", "OSC PWM",
              "OSC PW", "PENV A", "PENV D", "PENV DEPTH", "CUTOFF", "RESONANCE", "FENV A", "FENV D", "FENV S",
              "FENV R", "FENV DEPTH", "AMP LEVEL", "AENV A", "AENV D", "AENV S", "AENV R", "FX CTRL", "PORTA TIME",
              "BEND", "MODULATION", "CUTOFF KF", "FX LEVEL"]
DIST_TYPES = ["OFF", "DIST", "FUZZ", "BIT CRASH"]
FLG_TYPES = ["OFF", "FLANGER", "PHASER", "PITCH SHIFT"]
DLY_TYPES = ["OFF", "DELAY", "PAN DELAY"]
REV_TYPES = ["OFF", "ROOM", "HALL", "PLATE"]
ARP_GRIDS = ["4_", "8_", "8L", "8H", "8t", "16_", "16L", "16H", "16t"]
ARP_DURS = ["30", "40", "50", "60", "70", "80", "90", "100", "120", "FUL"]
ARP_MOTIFS = ["UP/L", "UP/L&H", "UP", "DOWN/L", "DOWN/L&H", "DOWN", "UP&DN/L", "UP&DN/L&H", "UP&DN", "RND/L", "RND",
              "PHRASE"]
RND_TYPES = ["PAD", "LEAD", "BASS", "PLUCK", "KEYS", "BRASS", "STRINGS", "FX"]


def P(key, name, lo=0, hi=127, d=0, opts=None, kind="PK_PATCH", **extra):
    p = {"key": key, "name": name, "kind": kind, "default": d}
    if opts:
        p["options"] = opts
        p["min"], p["max"] = 0, len(opts) - 1
    else:
        p["min"], p["max"] = lo, hi
        p["display"] = "int"
    p.update(extra)
    return p


def tone(t, on):
    k = "t%d_" % t
    n = "T%d " % t
    return [
        P(k + "on", n + "ON", d=on, opts=ONOFF),
        # OSC
        P(k + "wave", n + "WAVE", d=0, opts=WAVES),
        P(k + "var", n + "VARIATION", d=0, opts=VARS),
        P(k + "pitch", n + "PITCH", -24, 24, 0),
        P(k + "detune", n + "DETUNE", -50, 50, 0),
        P(k + "pwm", n + "PWM", 0, 127, 0),
        P(k + "pw", n + "PW", 0, 127, 0),
        P(k + "pa", n + "PITCH ENV A", 0, 127, 0),
        P(k + "pd", n + "PITCH ENV D", 0, 127, 0),
        P(k + "pdepth", n + "PITCH ENV DEPTH", -63, 63, 0),
        # FILTER
        P(k + "fmode", n + "FILTER MODE", d=0, opts=FMODES),
        P(k + "slope", n + "SLOPE", d=1, opts=SLOPES),
        P(k + "cutoff", n + "CUTOFF", 0, 127, 127),
        P(k + "reso", n + "RESONANCE", 0, 127, 0),
        P(k + "kf", n + "KEY FOLLOW", -100, 100, 0),
        P(k + "fa", n + "FILTER ENV A", 0, 127, 0),
        P(k + "fd", n + "FILTER ENV D", 0, 127, 64),
        P(k + "fs", n + "FILTER ENV S", 0, 127, 127),
        P(k + "fr", n + "FILTER ENV R", 0, 127, 0),
        P(k + "fdepth", n + "FILTER ENV DEPTH", -63, 63, 0),
        P(k + "fvelo", n + "FILTER VELO SENS", -63, 63, 0),
        # AMP
        P(k + "level", n + "LEVEL", 0, 127, 100),
        P(k + "aa", n + "AMP ENV A", 0, 127, 0),
        P(k + "ad", n + "AMP ENV D", 0, 127, 64),
        P(k + "as", n + "AMP ENV S", 0, 127, 127),
        P(k + "ar", n + "AMP ENV R", 0, 127, 10),
        P(k + "avelo", n + "AMP VELO SENS", -63, 63, 0),
        P(k + "pan", n + "PAN", -64, 63, 0),
        # LFO
        P(k + "lshape", n + "LFO SHAPE", d=0, opts=LFO_SHAPES),
        P(k + "lrate", n + "LFO RATE", 0, 127, 76),
        P(k + "lsync", n + "LFO TEMPO SYNC", d=0, opts=ONOFF),
        P(k + "lnote", n + "LFO SYNC NOTE", d=11, opts=SYNC_NOTES),
        P(k + "lfade", n + "LFO FADE TIME", 0, 127, 0),
        P(k + "lkey", n + "LFO KEY TRIG", d=0, opts=ONOFF),
        P(k + "lpitch", n + "LFO PITCH DEPTH", -63, 63, 0),
        P(k + "lfilter", n + "LFO FILTER DEPTH", -63, 63, 0),
        P(k + "lamp", n + "LFO AMP DEPTH", -63, 63, 0),
        P(k + "lpan", n + "LFO PAN DEPTH", -63, 63, 0),
        # MOD LFO (the modulation lever's LFO)
        P(k + "mshape", n + "MOD LFO SHAPE", d=0, opts=LFO_SHAPES),
        P(k + "mrate", n + "MOD LFO RATE", 0, 127, 80),
        P(k + "msync", n + "MOD LFO SYNC", d=0, opts=ONOFF),
        P(k + "mnote", n + "MOD LFO NOTE", d=11, opts=SYNC_NOTES),
        P(k + "mpitch", n + "MOD LFO PITCH", -63, 63, 10),
        P(k + "mfilter", n + "MOD LFO FILTER", -63, 63, 0),
        P(k + "mamp", n + "MOD LFO AMP", -63, 63, 0),
        P(k + "mpan", n + "MOD LFO PAN", -63, 63, 0),
    ]


PARAMS = tone(1, 1) + tone(2, 0) + tone(3, 0) + [
    # COMMON
    P("level", "PATCH LEVEL", 0, 127, 100),
    P("mono", "MONO", d=0, opts=ONOFF),
    P("porta", "PORTAMENTO", d=0, opts=ONOFF),
    P("ptime", "PORTA TIME", 0, 127, 16),
    P("octave", "PATCH OCTAVE", -3, 3, 0),
    P("bend_up", "BEND RANGE UP", 0, 24, 2),
    P("bend_dn", "BEND RANGE DOWN", 0, 24, 2),
    P("mod", "TONE 1/2 MOD", d=0, opts=MODS),
    P("fx_on", "EFFECTS ON/OFF", d=1, opts=ONOFF),
    P("boost", "LOW BOOST", d=0, opts=ONOFF),
    P("beam1_dest", "BEAM 1 ASSIGN", d=12, opts=BEAM_DESTS),
    P("beam1", "BEAM 1", 0, 127, 0),
    P("beam2_dest", "BEAM 2 ASSIGN", d=13, opts=BEAM_DESTS),
    P("beam2", "BEAM 2", 0, 127, 0),
    # EFFECTS
    P("dist_type", "DISTORTION", d=0, opts=DIST_TYPES),
    P("dist_drive", "DIST DRIVE", 0, 127, 60),
    P("dist_tone", "DIST TONE", 0, 127, 80),
    P("dist_level", "DIST LEVEL", 0, 127, 90),
    P("flg_type", "FLANGER", d=0, opts=FLG_TYPES),
    P("flg_rate", "FLANGER RATE", 0, 127, 30),
    P("flg_depth", "FLANGER DEPTH", 0, 127, 70),
    P("flg_fb", "FLANGER FEEDBACK", 0, 127, 60),
    P("flg_pitch", "FLANGER MANUAL/PITCH", -24, 24, 0),
    P("flg_level", "FLANGER LEVEL", 0, 127, 64),
    P("dly_type", "DELAY", d=0, opts=DLY_TYPES),
    P("dly_sync", "DELAY TEMPO SYNC", d=1, opts=ONOFF),
    P("dly_time", "DELAY TIME", 0, 127, 60),
    P("dly_note", "DELAY SYNC NOTE", d=14, opts=SYNC_NOTES),
    P("dly_fb", "DELAY FEEDBACK", 0, 127, 50),
    P("dly_damp", "DELAY HIGH DAMP", 0, 127, 64),
    P("dly_level", "DELAY LEVEL", 0, 127, 50),
    P("rev_type", "REVERB", d=0, opts=REV_TYPES),
    P("rev_time", "REVERB TIME", 0, 127, 70),
    P("rev_damp", "REVERB HIGH DAMP", 0, 127, 64),
    P("rev_level", "REVERB LEVEL", 0, 127, 50),
    # ARPEGGIO
    P("arp_on", "ARPEGGIO", d=0, opts=ONOFF),
    P("arp_hold", "KEY HOLD", d=0, opts=ONOFF),
    P("arp_grid", "ARP GRID", d=5, opts=ARP_GRIDS),
    P("arp_dur", "ARP DURATION", d=4, opts=ARP_DURS),
    P("arp_motif", "ARP MOTIF", d=2, opts=ARP_MOTIFS),
    P("arp_oct", "ARP OCTAVE", -3, 3, 0),
    P("arp_accent", "ARP ACCENT", 0, 100, 50),
    P("arp_velo", "ARP VELOCITY", 0, 127, 0),
    # the patch (VST programs), the randomizer
    P("patch", "PATCH", 0, 63, 0, kind="PK_UI"),          # the factory patches: src/patches.c patch_count() - 1
    P("patch_prev", "PATCH <", kind="PK_TRIG", momentary=True, step_of="patch", step_delta=-1),
    P("patch_next", "PATCH >", kind="PK_TRIG", momentary=True, step_of="patch", step_delta=1),
    P("patch_name", "PATCH NAME", 0, 0, 0, kind="PK_SHOW", display="string"),
    P("oct_dn", "OCTAVE DOWN", kind="PK_TRIG", momentary=True, step_of="octave", step_delta=-1),
    P("oct_up", "OCTAVE UP", kind="PK_TRIG", momentary=True, step_of="octave", step_delta=1),
    P("rnd_type", "RANDOM TYPE", d=0, opts=RND_TYPES, kind="PK_UI"),
    P("rnd_amt", "RANDOM AMOUNT", 0, 100, 100, kind="PK_UI"),
    P("rnd_go", "RANDOMIZE", kind="PK_TRIG", momentary=True),
]

for p in PARAMS:   # momentary keys: a plain 0..1 with no int display
    if p.get("momentary"):
        p["min"], p["max"] = 0, 1
        p.pop("display", None)


def params_json():
    out = []
    for p in PARAMS:
        q = {k: v for k, v in p.items() if k != "kind"}
        out.append(q)
    return json.dumps({"name": "GAIAXXL", "params": out}, indent=1) + "\n"


def cname(key):
    return "P_" + key.upper()


def header():
    L = ["/* GENERATED by tools/gen_params.py: do not edit. GAIAXXL's parameters, in VST index order. */",
         "#pragma once", "", "typedef enum { PK_PATCH, PK_UI, PK_TRIG, PK_SHOW } ParamKind;", "",
         "typedef struct { const char *key; ParamKind kind; float min, max, def; int nopts; } ParamDef;", "",
         "enum {"]
    for p in PARAMS:
        L.append("    %s," % cname(p["key"]))
    L += ["    P_COUNT", "};", "", "/* the first parameter of each tone; a tone's parameters follow in the same order */",
          "#define TONE_PARAMS %d" % len(tone(1, 0)), "#define TONE_BASE(t) (P_T1_ON + (t) * TONE_PARAMS)",
          "#define TP(t, name) (TONE_BASE(t) + (P_T1_##name - P_T1_ON))", "",
          "static const ParamDef PDEF[P_COUNT] = {"]
    for p in PARAMS:
        L.append('    {"%s", %s, %d, %d, %g, %d},' % (p["key"], p["kind"], p["min"], p["max"], p["default"],
                                                      len(p.get("options") or [])))
    L += ["};", ""]
    for name, lst in [("WAVE", WAVES), ("FMODE", FMODES), ("LSHAPE", LFO_SHAPES), ("SYNC_NOTE", SYNC_NOTES),
                      ("BEAM", BEAM_DESTS), ("DIST", DIST_TYPES), ("FLG", FLG_TYPES), ("DLY", DLY_TYPES),
                      ("REV", REV_TYPES), ("GRID", ARP_GRIDS), ("MOTIF", ARP_MOTIFS), ("RND", RND_TYPES)]:
        L.append("#define N_%s %d" % (name, len(lst)))
    L.append("")
    L.append("static const char *const RND_NAMES[N_RND] = {%s};" % ", ".join('"%s"' % s for s in RND_TYPES))
    return "\n".join(L) + "\n"


if __name__ == "__main__":
    targets = {os.path.join(ROOT, "params.json"): params_json(), os.path.join(ROOT, "src", "params_gen.h"): header()}
    if "--check" in sys.argv:
        for f, t in targets.items():
            if not os.path.exists(f) or open(f).read() != t:
                raise SystemExit("%s is stale: run python3 tools/gen_params.py" % f)
    else:
        for f, t in targets.items():
            open(f, "w").write(t)
        print("%d parameters" % len(PARAMS))
