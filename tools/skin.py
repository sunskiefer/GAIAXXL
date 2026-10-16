#!/usr/bin/env python3
"""GAIAXXL's six pages: their geometry, their artwork and every control in MPC's skin (TUI.json).

    python3 tools/skin.py --layout              writes layout.conf (the tabs and their Q-Links, for mpc-vst-plugins)
    python3 tools/skin.py "<skin>/Plugin Skins" builds the pages into the skin gen_vst.py made from layout.conf
    python3 tools/skin.py --preview DIR         draws each page with its controls at their defaults (offline check)

Pages (1280 x 628, MPC's whole plugin area): TONE 1, TONE 2, TONE 3, MOD LFO, EFFECTS, ARP/COMMON. A column on the
left of every page keeps the performance controls at hand (MONO, PORTAMENTO, PORTA TIME, OCTAVE, ARPEGGIO, EFFECTS,
LEVEL, RANDOM); a bar along the top shows the patch with its arrows.

Look: the colours of the GAIA editor (sampled from it): dark warm grey panels, amber frames (blue for the LFOs), light
grey slabs, black ground, LCD-cream value windows, red keys. The caps: tools/caps.py (aluminium-top knobs, a pointer on
parameter knobs and none on data knobs; fader caps white for envelopes, grey for modulation depths, black for levels
and the rest). The lettering: DejaVu Sans Condensed Bold (art/fonts), never smaller than 16 px; values are MPC's own
text (Titillium Web) at 21 px in the LCD windows. No Roland or GAIA marks anywhere."""
import json
import math
import os
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import caps  # noqa: E402
import gen_params  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT = os.path.join(ROOT, "art", "fonts", "DejaVuSansCondensed-Bold.ttf")
W, H = 1280, 628
Y0 = 86            # Force Shadow's y of the plugin area's top (layout.conf coordinates)
SS = 3             # supersampling for drawn shapes

# ---------------------------------------------------------------- colours (sampled from the GAIA editor)
BG = (0, 0, 0)
SLAB = (201, 201, 203)
PANEL = (90, 86, 85)
PANEL_DIM = (44, 42, 43)
AMBER = (243, 170, 59)
BLUE = (158, 187, 226)
LCD = (222, 227, 176)
LCD_EDGE = (120, 124, 92)
LCD_INK = "1a1c06"
INK = (240, 238, 234)
INK_DIM = (190, 186, 182)
TAB_INK = (26, 18, 8)
RED = (212, 62, 64)
RED_LIT = (255, 120, 118)
LED_OFF = (70, 26, 26)
LED_ON = (255, 64, 60)
KEY_FACE = (58, 56, 56)
SLOT = (24, 22, 23)
TICK = (190, 186, 184)

KNOB_FRAMES = 128
KNOB_SPAN = 150
MAX_STRIP = 12288

TABS = ["TONE 1", "TONE 2", "TONE 3", "MOD LFO", "EFFECTS", "ARP/COMMON"]
PARAMS = gen_params.PARAMS
INDEX = {p["key"]: i for i, p in enumerate(PARAMS)}
PBYKEY = {p["key"]: p for p in PARAMS}

_fonts = {}


def font(px):
    if px not in _fonts:
        _fonts[px] = ImageFont.truetype(FONT, px)
    return _fonts[px]


def R(v):
    return int(round(v))


# ---------------------------------------------------------------- the page model
class Page:
    """a page: its background drawing and its live controls"""

    def __init__(self, name):
        self.name = name
        self.bg = Image.new("RGB", (W, H), BG)
        self.d = ImageDraw.Draw(self.bg)
        self.controls = []     # dicts: kind, key, rect, extra

    def add(self, kind, key, rect, **kw):
        assert key in INDEX, key
        c = dict(kind=kind, key=key, rect=tuple(R(v) for v in rect))
        c.update(kw)
        self.controls.append(c)
        return c


# ---------------------------------------------------------------- drawing helpers (on the background)
def rrect(pg, rect, fill=None, outline=None, width=1, radius=8):
    x, y, w, h = rect
    big = Image.new("RGBA", (R(w * SS), R(h * SS)), (0, 0, 0, 0))
    ImageDraw.Draw(big).rounded_rectangle((0, 0, w * SS - 1, h * SS - 1), radius=radius * SS,
                                          fill=fill + (255,) if fill else None,
                                          outline=outline + (255,) if outline else None, width=R(width * SS))
    sm = big.resize((R(w), R(h)), Image.LANCZOS)
    base = pg.bg.convert("RGBA")
    base.alpha_composite(sm, (R(x), R(y)))
    pg.bg = base.convert("RGB")
    pg.d = ImageDraw.Draw(pg.bg)


def text(pg, xy, s, px=16, fill=INK, anchor="mm"):
    pg.d.text(xy, s, font=font(px), fill=fill, anchor=anchor)


def fit_text(pg, rect, s, px=16, fill=INK):
    """centred in rect, condensed sideways if the word is wider (never smaller than px)"""
    x, y, w, h = rect
    f = font(px)
    tw = f.getlength(s)
    if tw <= w - 4:
        text(pg, (x + w / 2, y + h / 2), s, px, fill)
        return
    big = Image.new("RGBA", (R(tw) + 8, px * 2), (0, 0, 0, 0))
    ImageDraw.Draw(big).text((4, px), s, font=f, fill=fill + (255,), anchor="lm")
    big = big.resize((R(w - 4), px * 2), Image.LANCZOS)
    base = pg.bg.convert("RGBA")
    base.alpha_composite(big, (R(x + 2), R(y + h / 2 - px)))
    pg.bg = base.convert("RGB")
    pg.d = ImageDraw.Draw(pg.bg)


def panel(pg, rect, title, accent=AMBER, dim=False):
    """a section: dark warm grey, a frame in the accent and a title tab"""
    x, y, w, h = rect
    rrect(pg, rect, fill=PANEL_DIM if dim else PANEL, outline=accent, width=2, radius=10)
    tw = font(20).getlength(title) + 26
    rrect(pg, (x + 8, y + 6, tw, 28), fill=accent, radius=7)
    text(pg, (x + 8 + tw / 2, y + 20), title, 20, TAB_INK)


def slab(pg, rect):
    rrect(pg, rect, fill=SLAB, radius=14)


def lcd(pg, rect):
    rrect(pg, rect, fill=LCD, outline=LCD_EDGE, width=1.5, radius=4)


def knob_scale(pg, cx, cy, r, bipolar=False):
    """the ticks around a parameter knob"""
    for k in range(11):
        a = math.radians(-KNOB_SPAN + 2 * KNOB_SPAN * k / 10 - 90)
        r0, r1 = r + 3, r + (9 if k in (0, 10) or (bipolar and k == 5) else 6)
        pg.d.line((cx + r0 * math.cos(a), cy + r0 * math.sin(a), cx + r1 * math.cos(a), cy + r1 * math.sin(a)),
                  fill=TICK, width=2)


def slot(pg, cx, y0, y1, bipolar=False):
    """a fader's slot and its ticks"""
    rrect(pg, (cx - 6, y0, 12, y1 - y0), fill=SLOT, radius=5)
    for k in range(11):
        y = y0 + 8 + (y1 - y0 - 16) * k / 10
        ln = 12 if k in (0, 10) or (bipolar and k == 5) else 7
        pg.d.line((cx - 10 - ln, y, cx - 10, y), fill=TICK, width=2)
        pg.d.line((cx + 10, y, cx + 10 + ln, y), fill=TICK, width=2)


# ---------------------------------------------------------------- live controls (registered, drawn by build())
def knob(pg, key, cx, cy, size, label=None, endless=False, lcd_rect=None, label_px=16, label_fill=INK, bipolar=None,
         label_w=96):
    if bipolar is None:
        p = PBYKEY[key]
        bipolar = p.get("min", 0) < 0
    if label:
        fit_text(pg, (cx - label_w / 2, cy - size / 2 - 27, label_w, 22), label, label_px, label_fill)
    if not endless:
        knob_scale(pg, cx, cy, size / 2, bipolar)
    pg.add("knob", key, (cx - size / 2, cy - size / 2, size, size), endless=endless)
    if lcd_rect is None:
        lcd_rect = (cx - 38, cy + size / 2 + 8, 76, 26)
    if lcd_rect:
        value(pg, key, lcd_rect)


def value(pg, key, rect):
    lcd(pg, rect)
    pg.add("value", key, rect)


CAP_COLOURS = {"white": ((236, 234, 228), (25, 25, 25)), "grey": ((172, 172, 170), (25, 25, 25)),
               "black": ((34, 33, 33), (245, 245, 245))}


def fader(pg, key, cx, top, bottom, label, cap="white", label_fill=INK, cap_w=44):
    """label, value window, then the fader (its slot drawn here, the cap a filmstrip)"""
    p = PBYKEY[key]
    fit_text(pg, (cx - 32, top, 64, 20), label, 16, label_fill)
    value(pg, key, (cx - 28, top + 22, 56, 26))
    y0 = top + 54
    slot(pg, cx, y0, bottom, p.get("min", 0) < 0)
    pg.add("slider", key, (cx - 30, y0 - 2, 60, bottom - y0 + 4), cap=cap, cap_w=cap_w)


def radio(pg, key, rects, words, style="led", px=16):
    """an option list: one picture pair per option; style led (a LED and the word) or key (a key lit red)"""
    for i, (r, w) in enumerate(zip(rects, words)):
        pg.add("opt", key, r, index=i, count=len(rects), word=w, style=style, px=px)


def toggle(pg, key, rect, word, px=16, colour=RED):
    pg.add("key", key, rect, word=word, px=px, colour=colour)


def momentary(pg, key, rect, word, px=16, arrow=None):
    pg.add("key", key, rect, word=word, px=px, colour=RED, arrow=arrow)


def readout(pg, key, rect, px=22):
    lcd(pg, rect)
    pg.add("value", key, rect, px=px, left=True)


# ---------------------------------------------------------------- the common parts
def left_column(pg, title, tone=None):
    text(pg, (64, 27), title, 24, INK)
    y = 54
    if tone is not None:
        toggle(pg, "t%d_on" % tone, (6, y, 116, 38), "TONE ON", 18)
    y = 100
    rrect(pg, (2, y - 4, 124, 470), outline=SLAB, width=2, radius=10)
    toggle(pg, "mono", (10, y + 4, 108, 36), "MONO", 17)
    toggle(pg, "porta", (10, y + 46, 108, 36), "PORTA", 17)
    knob(pg, "ptime", 64, y + 148, 54, "PORTA TIME", lcd_rect=(26, y + 182, 76, 26))
    text(pg, (64, y + 234), "OCTAVE", 16)
    momentary(pg, "oct_dn", (8, y + 248, 34, 34), "", arrow="down")
    value(pg, "octave", (45, y + 252, 38, 26))
    momentary(pg, "oct_up", (86, y + 248, 34, 34), "", arrow="up")
    toggle(pg, "arp_on", (10, y + 290, 108, 34), "ARPEGGIO", 16)
    toggle(pg, "fx_on", (10, y + 330, 108, 34), "EFFECTS", 16)
    knob(pg, "level", 64, y + 410, 46, "LEVEL", lcd_rect=(26, y + 438, 76, 24))
    momentary(pg, "rnd_go", (6, 574, 116, 48), "RANDOM", 19)


def top_bar(pg):
    momentary(pg, "patch_prev", (136, 6, 48, 42), "", arrow="left")
    readout(pg, "patch_name", (190, 6, 420, 42), 24)
    momentary(pg, "patch_next", (616, 6, 48, 42), "", arrow="right")
    text(pg, (680, 27), "PATCH", 18, INK_DIM, "lm")


# ---------------------------------------------------------------- the pages
def tone_page(t):
    pg = Page("TONE %d" % (t + 1))
    k = "t%d_" % (t + 1)
    left_column(pg, "TONE %d" % (t + 1), t + 1)
    top_bar(pg)
    if t == 1:   # TONE 2: the TONE 1 -> 2 modulation lives here (it acts on tone 2's oscillator)
        text(pg, (900, 27), "TONE 1 > 2", 18, INK, "rm")
        radio(pg, "mod", [(910 + 120 * i, 8, 114, 38) for i in range(3)], ["OFF", "SYNC", "RING"], "key", 17)
    slab(pg, (136, 56, 1140, 568))
    top, sl_top, sl_bot = 64, 300, 608
    # ---- LFO
    x0, w = 144, 236
    panel(pg, (x0, top, w, 552), "LFO", BLUE)
    radio(pg, k + "lshape", [(x0 + 6, 102 + 30 * i, 96, 28) for i in range(6)], gen_params.LFO_SHAPES)
    knob(pg, k + "lrate", x0 + 168, 150, 64, "RATE", lcd_rect=(x0 + 130, 188, 76, 26))
    toggle(pg, k + "lsync", (x0 + 108, 216, 60, 30), "SYNC", 16)
    toggle(pg, k + "lkey", (x0 + 172, 216, 58, 30), "KEY", 16)
    knob(pg, k + "lnote", x0 + 128, 274, 34, None, endless=True, lcd_rect=(x0 + 150, 262, 80, 26))
    for i, (key, lab, cap, lf) in enumerate([("lfade", "FADE", "black", INK), ("lpitch", "PITCH", "grey", AMBER),
                                             ("lfilter", "FILTER", "grey", AMBER), ("lamp", "AMP", "grey", AMBER)]):
        fader(pg, k + key, x0 + 30 + i * 58.7, sl_top, sl_bot, lab, cap, lf)
    # ---- OSC
    x0, w = 386, 322
    panel(pg, (x0, top, w, 552), "OSC")
    radio(pg, k + "wave", [(x0 + 6, 100 + 27.5 * i, 112, 26) for i in range(7)], gen_params.WAVES)
    text(pg, (x0 + 220, 112), "VARIATION", 16)
    radio(pg, k + "var", [(x0 + 128 + 63 * i, 124, 58, 32) for i in range(3)], gen_params.VARS, "key", 18)
    knob(pg, k + "pitch", x0 + 172, 214, 58, "PITCH", lcd_rect=(x0 + 134, 250, 76, 26))
    knob(pg, k + "detune", x0 + 268, 214, 58, "DETUNE", lcd_rect=(x0 + 230, 250, 76, 26))
    for i, (key, lab, cap, lf) in enumerate([("pwm", "PWM", "grey", INK), ("pw", "PW", "black", INK),
                                             ("pa", "ENV A", "white", INK), ("pd", "ENV D", "white", INK),
                                             ("pdepth", "DEPTH", "grey", AMBER)]):
        fader(pg, k + key, x0 + 33 + i * 64, sl_top, sl_bot, lab, cap, lf)
    # ---- FILTER
    x0, w = 714, 322
    panel(pg, (x0, top, w, 552), "FILTER")
    radio(pg, k + "fmode", [(x0 + 6, 100 + 30 * i, 106, 28) for i in range(5)], gen_params.FMODES)
    radio(pg, k + "slope", [(x0 + 6 + 54 * i, 256, 50, 32) for i in range(2)], ["-12", "-24"], "key", 17)
    knob(pg, k + "cutoff", x0 + 170, 150, 62, "CUTOFF", lcd_rect=(x0 + 132, 188, 76, 26))
    knob(pg, k + "reso", x0 + 268, 150, 62, "RESONANCE", lcd_rect=(x0 + 230, 188, 76, 26))
    knob(pg, k + "kf", x0 + 150, 260, 40, None, lcd_rect=(x0 + 232, 248, 76, 26))
    text(pg, (x0 + 270, 232), "KEY FOLLOW", 16)
    for i, (key, lab, cap, lf) in enumerate([("fa", "A", "white", INK), ("fd", "D", "white", INK), ("fs", "S", "white", INK),
                                             ("fr", "R", "white", INK), ("fdepth", "DEPTH", "grey", AMBER)]):
        fader(pg, k + key, x0 + 33 + i * 64, sl_top, sl_bot, lab, cap, lf)
    # ---- AMP
    x0, w = 1042, 226
    panel(pg, (x0, top, w, 552), "AMP")
    knob(pg, k + "level", x0 + 113, 178, 96, "LEVEL", lcd_rect=(x0 + 75, 238, 76, 26))
    for i, (key, lab) in enumerate([("aa", "A"), ("ad", "D"), ("as", "S"), ("ar", "R")]):
        fader(pg, k + key, x0 + 29 + i * 56, sl_top, sl_bot, lab, "white", INK, cap_w=42)
    return pg


def mod_page():
    pg = Page("MOD LFO")
    left_column(pg, "MOD LFO")
    top_bar(pg)
    text(pg, (1270, 27), "MOD LEVER LFO, VELOCITY, PAN", 17, INK_DIM, "rm")
    slab(pg, (136, 56, 1140, 568))
    for t in range(3):
        k = "t%d_" % (t + 1)
        y = 64 + t * 186
        panel(pg, (144, y, 1124, 178), "TONE %d" % (t + 1), BLUE)
        radio(pg, k + "mshape", [(152 + 62 * (i % 3), y + 44 + 32 * (i // 3), 58, 30) for i in range(6)],
              gen_params.LFO_SHAPES, "key", 16)
        toggle(pg, k + "msync", (152, y + 112, 182, 32), "TEMPO SYNC", 16)
        cols = [("mrate", "RATE", False, INK), ("mnote", "NOTE", True, INK), ("mpitch", "PITCH", False, AMBER),
                ("mfilter", "FILTER", False, AMBER), ("mamp", "AMP", False, AMBER), ("mpan", "PAN", False, AMBER)]
        for i, (key, lab, endl, lf) in enumerate(cols):
            cx = 392 + i * 86
            knob(pg, k + key, cx, y + 88, 50, lab, endless=endl, lcd_rect=(cx - 38, y + 122, 76, 26), label_fill=lf)
        pg.d.line((902, y + 40, 902, y + 166), fill=SLAB, width=2)
        text(pg, (930, y + 20), "LFO PAN / AMP PAN / VELOCITY", 16, INK, "lm")
        for i, (key, lab) in enumerate([("lpan", "LFO PAN"), ("pan", "PAN"), ("fvelo", "F VELO"), ("avelo", "A VELO")]):
            cx = 952 + i * 86
            knob(pg, k + key, cx, y + 88, 50, lab, lcd_rect=(cx - 38, y + 122, 76, 26))
    return pg


def fx_page():
    pg = Page("EFFECTS")
    left_column(pg, "EFFECTS")
    top_bar(pg)
    slab(pg, (136, 56, 1140, 568))
    # DISTORTION
    x0, y0 = 144, 64
    panel(pg, (x0, y0, 400, 272), "DISTORTION")
    radio(pg, "dist_type", [(x0 + 8, y0 + 44 + 34 * i, 120, 30) for i in range(4)], gen_params.DIST_TYPES)
    for i, (key, lab) in enumerate([("dist_drive", "DRIVE"), ("dist_tone", "TONE"), ("dist_level", "LEVEL")]):
        knob(pg, key, x0 + 180 + i * 82, y0 + 110, 58, lab, lcd_rect=(x0 + 142 + i * 82, y0 + 148, 76, 26))
    toggle(pg, "boost", (x0 + 150, y0 + 200, 236, 40), "LOW BOOST", 18)
    # FLANGER
    x0 = 552
    panel(pg, (x0, y0, 716, 272), "FLANGER")
    radio(pg, "flg_type", [(x0 + 8, y0 + 44 + 34 * i, 150, 30) for i in range(4)], gen_params.FLG_TYPES)
    for i, (key, lab) in enumerate([("flg_rate", "RATE"), ("flg_depth", "DEPTH"), ("flg_fb", "FEEDBACK"),
                                    ("flg_pitch", "MANUAL/PITCH"), ("flg_level", "LEVEL")]):
        knob(pg, key, x0 + 220 + i * 104, y0 + 110, 58, lab, lcd_rect=(x0 + 182 + i * 104, y0 + 148, 76, 26), label_w=100)
    # DELAY
    x0, y0 = 144, 344
    panel(pg, (x0, y0, 660, 272), "DELAY")
    radio(pg, "dly_type", [(x0 + 8, y0 + 44 + 34 * i, 132, 30) for i in range(3)], gen_params.DLY_TYPES)
    toggle(pg, "dly_sync", (x0 + 8, y0 + 160, 132, 36), "TEMPO SYNC", 16)
    for i, (key, lab, endl) in enumerate([("dly_time", "TIME", False), ("dly_note", "NOTE", True), ("dly_fb", "FEEDBACK", False),
                                          ("dly_damp", "HIGH DAMP", False), ("dly_level", "LEVEL", False)]):
        knob(pg, key, x0 + 200 + i * 100, y0 + 110, 58, lab, endless=endl, lcd_rect=(x0 + 162 + i * 100, y0 + 148, 76, 26))
    # REVERB
    x0 = 812
    panel(pg, (x0, y0, 456, 272), "REVERB")
    radio(pg, "rev_type", [(x0 + 8, y0 + 44 + 34 * i, 120, 30) for i in range(4)], gen_params.REV_TYPES)
    for i, (key, lab) in enumerate([("rev_time", "TIME"), ("rev_damp", "HIGH DAMP"), ("rev_level", "LEVEL")]):
        knob(pg, key, x0 + 196 + i * 96, y0 + 110, 58, lab, lcd_rect=(x0 + 158 + i * 96, y0 + 148, 76, 26))
    return pg


def arp_page():
    pg = Page("ARP/COMMON")
    left_column(pg, "ARP/COM")
    top_bar(pg)
    slab(pg, (136, 56, 1140, 568))
    # ARPEGGIO
    x0, y0 = 144, 64
    panel(pg, (x0, y0, 756, 300), "ARPEGGIO")
    toggle(pg, "arp_hold", (x0 + 170, y0 + 6, 130, 30), "KEY HOLD", 16)
    text(pg, (x0 + 12, y0 + 56), "GRID", 16, INK, "lm")
    radio(pg, "arp_grid", [(x0 + 112 + 62 * i, y0 + 42, 58, 30) for i in range(9)], gen_params.ARP_GRIDS, "key", 16)
    text(pg, (x0 + 12, y0 + 100), "DURATION", 16, INK, "lm")
    radio(pg, "arp_dur", [(x0 + 112 + 56 * i, y0 + 86, 52, 30) for i in range(10)], gen_params.ARP_DURS, "key", 16)
    text(pg, (x0 + 12, y0 + 146), "MOTIF", 16, INK, "lm")
    radio(pg, "arp_motif", [(x0 + 112 + 102 * (i % 6), y0 + 130 + 36 * (i // 6), 98, 32) for i in range(12)],
          gen_params.ARP_MOTIFS, "key", 16)
    for i, (key, lab, endl) in enumerate([("arp_oct", "OCTAVE", True), ("arp_accent", "ACCENT", False),
                                          ("arp_velo", "VELOCITY", False)]):
        cx = x0 + 140 + i * 180
        knob(pg, key, cx, y0 + 240, 50, None, endless=endl, lcd_rect=(cx + 32, y0 + 228, 76, 26))
        text(pg, (cx + 70, y0 + 214), lab, 16)
    text(pg, (x0 + 740, y0 + 284), "VELOCITY 0 = REAL", 16, INK_DIM, "rm")
    # RANDOMIZER
    x0 = 908
    panel(pg, (x0, y0, 360, 300), "RANDOMIZER")
    radio(pg, "rnd_type", [(x0 + 10 + 86 * (i % 4), y0 + 46 + 36 * (i // 4), 82, 32) for i in range(8)],
          gen_params.RND_TYPES, "key", 16)
    knob(pg, "rnd_amt", x0 + 70, y0 + 186, 58, "AMOUNT", lcd_rect=(x0 + 32, y0 + 224, 76, 26))
    momentary(pg, "rnd_go", (x0 + 150, y0 + 150, 196, 80), "RANDOMIZE", 22)
    text(pg, (x0 + 248, y0 + 260), "keeps it musical", 16, INK_DIM)
    # COMMON
    x0, y0 = 144, 372
    panel(pg, (x0, y0, 420, 244), "COMMON")
    knob(pg, "bend_up", x0 + 70, y0 + 100, 54, "BEND UP", lcd_rect=(x0 + 32, y0 + 136, 76, 26))
    knob(pg, "bend_dn", x0 + 190, y0 + 100, 54, "BEND DOWN", lcd_rect=(x0 + 152, y0 + 136, 76, 26))
    text(pg, (x0 + 340, y0 + 56), "TONE 1 > 2", 16)
    radio(pg, "mod", [(x0 + 284, y0 + 72 + 36 * i, 112, 32) for i in range(3)], ["OFF", "SYNC", "RING"], "key", 17)
    # BEAMS
    x0 = 572
    panel(pg, (x0, y0, 696, 244), "BEAMS")
    for b in range(2):
        bx = x0 + 20 + b * 340
        text(pg, (bx + 70, y0 + 52), "BEAM %d ASSIGN" % (b + 1), 16)
        knob(pg, "beam%d_dest" % (b + 1), bx + 34, y0 + 104, 46, None, endless=True, lcd_rect=(bx + 66, y0 + 92, 160, 26))
        text(pg, (bx + 140, y0 + 150), "touch and slide", 16, INK_DIM)
        pg.add("hslider", "beam%d" % (b + 1), (bx, y0 + 170, 300, 60))
        rrect(pg, (bx + 6, y0 + 194, 288, 12), fill=SLOT, radius=5)
    return pg


PAGES = [lambda: tone_page(0), lambda: tone_page(1), lambda: tone_page(2), mod_page, fx_page, arp_page]

# ---------------------------------------------------------------- Q-Links: each page's knobs and faders, 8 per bank
QLINK_ORDER = {
    0: None, 1: None, 2: None,
    3: [],
    4: ["dist_drive", "dist_tone", "dist_level", "flg_rate", "flg_depth", "flg_fb", "flg_pitch", "flg_level",
        "dly_time", "dly_fb", "dly_damp", "dly_level", "rev_time", "rev_damp", "rev_level", "dly_note"],
    5: ["arp_oct", "arp_accent", "arp_velo", "rnd_amt", "bend_up", "bend_dn", "beam1", "beam2"],
}


def qlink_banks(tab):
    if tab < 3:
        k = "t%d_" % (tab + 1)
        banks = [["cutoff", "reso", "kf", "fdepth", "fa", "fd", "fs", "fr"],
                 ["pitch", "detune", "pw", "pwm", "pa", "pd", "pdepth", "level"],
                 ["aa", "ad", "as", "ar", "lrate", "lpitch", "lfilter", "lamp"],
                 ["lfade", "lnote"]]
        return [[k + x for x in b] for b in banks]
    if tab == 3:
        return [["t%d_%s" % (t + 1, x) for x in ("mrate", "mpitch", "mfilter", "mamp")] +
                ["t%d_%s" % (t + 1, x) for x in ("mpan", "lpan", "fvelo", "avelo")] for t in range(3)]
    keys = QLINK_ORDER[tab]
    return [keys[i:i + 8] for i in range(0, len(keys), 8)]


def layout_conf():
    L = ["# GAIAXXL -- page layout (mpc-vst-plugins layout.conf syntax: tools/shadow_skin.py).",
         "# GENERATED by tools/skin.py --layout: edit that. tools/skin.py then builds every control of the six pages.",
         "theme_bg=000000", "theme_panel=000000", "theme_ink=f0eeea", "theme_ink_dim=bebab6", "theme_accent=f3aa3b",
         "theme_knob_ring=f3aa3b", "theme_seg_active=f3aa3b", ""]
    for t, name in enumerate(TABS):
        L.append("[tab %s]" % name)
        banks = qlink_banks(t)
        for i, key in enumerate(k for b in banks for k in b):
            L.append('knob cx=%d cy=%d r=12 label="" key=%s ns=0 bw=30' % (24 + 34 * (i % 36), Y0 + 24 + 40 * (i // 36), key))
        for i, b in enumerate(banks):
            L.append('qlinks "%s %d" = %s' % (name, i + 1, ",".join(b)))
        L.append("")
    return "\n".join(L) + "\n"


# ---------------------------------------------------------------- pictures of the live controls
def knob_strip(cache, size, endless):
    key = ("knob", size, endless)
    if key not in cache:
        pic = caps.knob(size * 2, not endless)
        strip = Image.new("RGBA", (size, size * KNOB_FRAMES), (0, 0, 0, 0))
        for f in range(KNOB_FRAMES):
            ang = -KNOB_SPAN + 2 * KNOB_SPAN * f / (KNOB_FRAMES - 1)
            fr = pic.rotate(-ang, resample=Image.BICUBIC).resize((size, size), Image.LANCZOS)
            strip.paste(fr, (0, f * size))
        cache[key] = strip
    return cache[key]


def cap_pic(cache, colour, w):
    key = ("cap", colour, w)
    if key not in cache:
        body, line = CAP_COLOURS[colour]
        cache[key] = caps.slider(w, round(w * 64 / 48), body, line)
    return cache[key]


def slider_frames(bg, rect, cap, n_frames, horizontal=False):
    x, y, w, h = rect
    under = bg.crop((x, y, x + w, y + h)).convert("RGBA")
    frames = []
    for f in range(n_frames):
        v = f / (n_frames - 1)
        fr = under.copy()
        if horizontal:
            cx = 2 + cap.size[0] / 2 + (w - 4 - cap.size[0]) * v
            fr.alpha_composite(cap, (R(cx - cap.size[0] / 2), (h - cap.size[1]) // 2))
        else:
            cy = h - 2 - cap.size[1] / 2 - (h - 4 - cap.size[1]) * v
            fr.alpha_composite(cap, ((w - cap.size[0]) // 2, R(cy - cap.size[1] / 2)))
        frames.append(fr.convert("RGB"))
    return frames


def key_pics(bg, c):
    """(off, on) pictures of a key: a rounded key, its word, red when lit"""
    x, y, w, h = c["rect"]
    out = []
    for lit in (False, True):
        im = bg.crop((x, y, x + w, y + h)).convert("RGBA")
        big = Image.new("RGBA", (w * SS, h * SS), (0, 0, 0, 0))
        d = ImageDraw.Draw(big)
        face = c.get("colour", RED) if lit else KEY_FACE
        if lit:
            glow = Image.new("RGBA", (w * SS, h * SS), (0, 0, 0, 0))
            ImageDraw.Draw(glow).rounded_rectangle((2 * SS, 2 * SS, w * SS - 2 * SS, h * SS - 2 * SS), radius=7 * SS,
                                                   fill=RED_LIT + (160,))
            big.alpha_composite(glow.filter(ImageFilter.GaussianBlur(4 * SS)))
        d.rounded_rectangle((2 * SS, 2 * SS, w * SS - 3 * SS, h * SS - 3 * SS), radius=7 * SS, fill=face + (255,),
                            outline=(150, 146, 144, 255) if not lit else (255, 170, 170, 255), width=2 * SS)
        d.line((10 * SS, 4 * SS, w * SS - 11 * SS, 4 * SS), fill=(255, 255, 255, 70), width=SS)   # the top edge's light
        arrow = c.get("arrow")
        if arrow:
            cx, cy, a = w * SS / 2, h * SS / 2, min(w, h) * SS * 0.22
            pts = {"up": [(cx, cy - a), (cx - a, cy + a * 0.7), (cx + a, cy + a * 0.7)],
                   "down": [(cx, cy + a), (cx - a, cy - a * 0.7), (cx + a, cy - a * 0.7)],
                   "left": [(cx - a, cy), (cx + a * 0.7, cy - a), (cx + a * 0.7, cy + a)],
                   "right": [(cx + a, cy), (cx - a * 0.7, cy - a), (cx - a * 0.7, cy + a)]}[arrow]
            d.polygon(pts, fill=INK + (255,))
        word = c.get("word") or ""
        if word:
            f = font(c.get("px", 16) * SS)
            tw = f.getlength(word)
            if tw > (w - 8) * SS:   # condensed into the key, never smaller
                lay = Image.new("RGBA", (R(tw) + 4, f.size * 2), (0, 0, 0, 0))
                ImageDraw.Draw(lay).text((2, f.size), word, font=f, fill=INK + (255,), anchor="lm")
                lay = lay.resize(((w - 8) * SS, f.size * 2), Image.LANCZOS)
                big.alpha_composite(lay, (4 * SS, R(h * SS / 2 - f.size)))
            else:
                d.text((w * SS / 2, h * SS / 2), word, font=f, fill=INK + (255,), anchor="mm")
        im.alpha_composite(big.resize((w, h), Image.LANCZOS))
        out.append(im)
    return out


def opt_pics(bg, c):
    if c.get("style") == "key":
        return key_pics(bg, c)
    x, y, w, h = c["rect"]
    out = []
    for lit in (False, True):
        im = bg.crop((x, y, x + w, y + h)).convert("RGBA")
        big = Image.new("RGBA", (w * SS, h * SS), (0, 0, 0, 0))
        d = ImageDraw.Draw(big)
        r, cx, cy = 6 * SS, 12 * SS, h * SS / 2
        if lit:
            glow = Image.new("RGBA", big.size, (0, 0, 0, 0))
            ImageDraw.Draw(glow).ellipse((cx - 2 * r, cy - 2 * r, cx + 2 * r, cy + 2 * r), fill=LED_ON + (150,))
            big.alpha_composite(glow.filter(ImageFilter.GaussianBlur(4 * SS)))
        d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(LED_ON if lit else LED_OFF) + (255,), outline=(20, 8, 8, 255), width=SS)
        f = font(c.get("px", 16) * SS)
        d.text((24 * SS, cy), c["word"], font=f, fill=(INK if lit else INK_DIM) + (255,), anchor="lm")
        im.alpha_composite(big.resize((w, h), Image.LANCZOS))
        out.append(im)
    return out


# ---------------------------------------------------------------- TUI.json pieces (the formats as mpc-vst-plugins
# and LXRXXL write them: docs/NOTES.md of mpc-vst-plugins)
def bounds_s(x, y, w, h):
    return "%d %d %d %d" % (R(x), R(y), R(w), R(h))


def sub(name, typ, data, b, visible="Always"):
    return {"version": 2, "componentData": {"version": 1, "name": name, "type": typ, "data": data},
            "handle remapping": {"version": 1, "map": []},
            "bounds": {"version": 2, "acceptsHWFocus": "No", "showWhenDataModelInvalid": "Show", "whenVisible": visible,
                       "boundsType": "Absolute", "bounds": b, "additionalInvalidatingHandles": []}}


def focus(w, h):
    return sub("Focus", "Focus", {"version": 1, "backgroundColour": "00000000", "outlineColour": "00000000",
                                  "backgroundInset": 2.0, "outlineThickness": 0.0}, bounds_s(0, 0, w, h), "WhenFocussed")


def action(on, handler, extra=""):
    return {"version": 2, "onAction": on, "handler": handler, "handleName": "" if handler == "Show Overlay" else "Data",
            "additionalData": extra, "handle remapping": {"version": 1, "map": []}}


KNOB_ACTIONS = [action("Mouse Down", "Q-Link"), action("Double Click", "Show Overlay", "knob overlay"),
                action("Enter Pressed", "Show Overlay", "knob overlay")]
KEY_ACTIONS = [action("Mouse Down", "Q-Link"), action("Enter Pressed", "Toggle Switch")]
RADIO_ACTIONS = [action("Mouse Down", "Q-Link")]


def definition(key, actions, parts, ignore=False):
    return {"key": key, "value": {
        "version": 4, "actions": actions,
        "backgroundData": {"version": 1, "focussed": {"version": 1, "colour": "0", "image": ""},
                           "unfocussed": {"version": 1, "colour": "0", "image": ""}},
        "ignoreMousePresses": ignore, "disableCoarseDataWheel": False, "repeats": 1, "hideQLinkBounds": True,
        "componentsData": parts}}


def filmstrip(name, img, frames, w, h, drag="Vertical", x=0, y=0):
    return sub(name, "Knob", {"version": 5, "knobType": "FilmStrip", "filmStrip": img, "numFrames": frames,
                              "invert": False, "dragOrientation": drag, "handleName": "Data"}, bounds_s(x, y, w, h))


def button(on_img, off_img, bid, n, w, h):
    return sub("Button", "Button", {"version": 2, "onImage": on_img, "offImage": off_img, "buttonId": bid,
                                    "numButtonsInGroup": n, "handleName": "Data", "gestureBehaviour": "Instant"},
               bounds_s(0, 0, w, h))


def value_label(w, h, px, colour=LCD_INK, left=False):
    return sub("Value", "Label", {"version": 1, "textStyle": {
        "version": 1, "font": {"version": 1, "name": "Titillium Web", "style": "Bold", "height": px},
        "colour": "ff" + colour,
        "justification": ("left" if left else "horizontallyCentred") + " verticallyCentred", "case": "Original"},
        "type": "Value", "handleName": "Data"}, bounds_s(10 if left else 0, 0, w - (10 if left else 0), h))


def placed(dkey, name, index, rect, focus_on=True):
    x, y, w, h = rect
    return {"version": 2,
            "componentData": {"version": 1, "name": name, "type": dkey, "data": {"version": 1, "handleName": "Data"}},
            "handle remapping": {"version": 1, "map": [{"key": "Data", "value": "Parameter %d" % index}]},
            "bounds": {"version": 2, "acceptsHWFocus": "Yes" if focus_on else "No", "showWhenDataModelInvalid": "Hide",
                       "whenVisible": "Always", "boundsType": "Absolute", "bounds": bounds_s(x, y, w, h),
                       "additionalInvalidatingHandles": []}}


class Saver:
    def __init__(self, folder):
        self.folder = folder
        self.seen = {}
        self.used = set()

    def save(self, img, name):
        h = (img.mode, img.size, hash(img.tobytes()))
        if h in self.seen:
            return self.seen[h]
        img.save(os.path.join(self.folder, name))
        self.seen[h] = name
        self.used.add(name)
        return name


def build_page(pg, tab, sv, defs, cache):
    kids = []
    t = "p%d" % tab
    focus_seen = set()

    def place(dkey, name, key, rect):
        f = key not in focus_seen
        focus_seen.add(key)
        kids.append(placed(dkey, name, INDEX[key], rect, f))

    have = {d["key"] for d in defs}

    def add_def(dkey, *a, **kw):
        if dkey not in have:
            defs.append(definition(dkey, *a, **kw))
            have.add(dkey)

    for c in pg.controls:
        x, y, w, h = c["rect"]
        key = c["key"]
        if c["kind"] == "knob":
            strip = knob_strip(cache, w, c["endless"])
            name = sv.save(strip, "gx_knob_%d_%s.png" % (w, "e" if c["endless"] else "p"))
            tb = max(w, 56)
            dkey = "gxKnob_%d_%d_%s" % (w, tb, "e" if c["endless"] else "p")
            add_def(dkey, KNOB_ACTIONS, [focus(tb, tb), filmstrip("Knob", name, KNOB_FRAMES - 1, w, w, "Vertical",
                                                                  (tb - w) // 2, (tb - w) // 2)])
            place(dkey, key, key, (x - (tb - w) // 2, y - (tb - w) // 2, tb, tb))
        elif c["kind"] in ("slider", "hslider"):
            horiz = c["kind"] == "hslider"
            cap = cap_pic(cache, c.get("cap", "black"), c.get("cap_w", 44))
            if horiz:
                cap = cap.rotate(90, expand=True)
            n = min(101, MAX_STRIP // (w if horiz else h)) if not horiz else 101
            frames = slider_frames(pg.bg, c["rect"], cap, n, horiz)
            if horiz:   # a horizontal strip would be wide: frames stacked as usual
                strip = Image.new("RGB", (w, h * n))
                for i, fr in enumerate(frames):
                    strip.paste(fr, (0, i * h))
            else:
                strip = Image.new("RGB", (w, h * n))
                for i, fr in enumerate(frames):
                    strip.paste(fr, (0, i * h))
            name = sv.save(strip, "gx_%s_sl_%s.png" % (t, key))
            dkey = "gxSlider_%s_%s" % (t, key)
            add_def(dkey, KNOB_ACTIONS, [focus(w, h), filmstrip("Slider", name, n, w, h,
                                                                "Horizontal" if horiz else "Vertical")])
            place(dkey, key, key, c["rect"])
        elif c["kind"] == "opt":
            off, on = opt_pics(pg.bg, c)
            on_n = sv.save(on.convert("RGB"), "gx_%s_%s_%d_on.png" % (t, key, c["index"]))
            off_n = sv.save(off.convert("RGB"), "gx_%s_%s_%d_off.png" % (t, key, c["index"]))
            dkey = "gxOpt_%s_%s_%d" % (t, key, c["index"])
            add_def(dkey, RADIO_ACTIONS, [button(on_n, off_n, c["index"], c["count"], w, h)])
            place(dkey, "%s %d" % (key, c["index"]), key, c["rect"])
        elif c["kind"] == "key":
            off, on = key_pics(pg.bg, c)
            on_n = sv.save(on.convert("RGB"), "gx_%s_key_%s_on.png" % (t, key))
            off_n = sv.save(off.convert("RGB"), "gx_%s_key_%s_off.png" % (t, key))
            dkey = "gxKey_%s_%s" % (t, key)
            add_def(dkey, KEY_ACTIONS, [focus(w, h), button(on_n, off_n, 1, 1, w, h)])
            place(dkey, key, key, c["rect"])
        elif c["kind"] == "value":
            px = c.get("px", 21)
            dkey = "gxValue_%d_%d_%d_%d" % (w, h, px, 1 if c.get("left") else 0)
            add_def(dkey, [], [value_label(w, h, px, left=c.get("left", False))], ignore=True)
            kids.append(placed(dkey, key + " value", INDEX[key], c["rect"], False))
    return kids


def build(skin):
    tui_path = os.path.join(skin, "TUI.json")
    tui = json.load(open(tui_path))
    cdefs = tui["pageData"]["componentDefinitions"]
    defs = cdefs["localComponentDefinitions"]
    sv = Saver(skin)
    cache = {}
    newdefs, pages = [], {}
    for tab, make in enumerate(PAGES):
        pg = make()
        kids = build_page(pg, tab, sv, newdefs, cache)
        bgname = sv.save(pg.bg, "gx_bg_%d.png" % tab)
        pages[TABS[tab]] = [sub("Background", "Image", {"version": 2, "imageType": "Regular", "colour": "0", "image": bgname},
                                bounds_s(0, 0, W, H))] + kids
        # every Q-Link key has a control on its page
        bound = {k["handle remapping"]["map"][0]["value"] for k in kids if k["handle remapping"]["map"]}
        for b in qlink_banks(tab):
            for key in b:
                if "Parameter %d" % INDEX[key] not in bound:
                    raise SystemExit("skin: %s (a Q-Link of %s) has no control on the page" % (key, TABS[tab]))
    keep = []
    for e in defs:
        tabname = e["key"].split("|")[0]
        if "|" in e["key"] and tabname in pages:
            e["value"]["componentsData"] = pages[tabname]
            e["value"]["backgroundData"] = {"version": 1, "focussed": {"version": 1, "colour": "ff000000", "image": ""},
                                            "unfocussed": {"version": 1, "colour": "ff000000", "image": ""}}
            keep.append(e)
    cdefs["localComponentDefinitions"] = keep + newdefs
    json.dump(tui, open(tui_path, "w"), indent=1)
    for f in os.listdir(skin):
        if f.endswith(".png") and f not in sv.used and not f.startswith("browser"):
            os.remove(os.path.join(skin, f))
    print("skin: %d pages, %d definitions, %d pictures" % (len(pages), len(newdefs), len(sv.used)))


# ---------------------------------------------------------------- the offline preview
def default_norm(key):
    p = PBYKEY[key]
    if p.get("options"):
        return p["default"] / max(1, len(p["options"]) - 1)
    lo, hi = p.get("min", 0), p.get("max", 1)
    return (p["default"] - lo) / (hi - lo) if hi > lo else 0


def preview(out_dir):
    os.makedirs(out_dir, exist_ok=True)
    cache = {}
    for tab, make in enumerate(PAGES):
        pg = make()
        img = pg.bg.convert("RGBA")
        d = ImageDraw.Draw(img)
        for c in pg.controls:
            x, y, w, h = c["rect"]
            key = c["key"]
            n = default_norm(key)
            if c["kind"] == "knob":
                strip = knob_strip(cache, w, c["endless"])
                f = R(n * (KNOB_FRAMES - 1))
                img.alpha_composite(strip.crop((0, f * w, w, f * w + w)), (x, y))
            elif c["kind"] in ("slider", "hslider"):
                horiz = c["kind"] == "hslider"
                cap = cap_pic(cache, c.get("cap", "black"), c.get("cap_w", 44))
                if horiz:
                    cap = cap.rotate(90, expand=True)
                fr = slider_frames(pg.bg, c["rect"], cap, 2, horiz)
                frames = slider_frames(pg.bg, c["rect"], cap, 101, horiz)
                img.paste(frames[R(n * 100)], (x, y))
            elif c["kind"] == "opt":
                off, on = opt_pics(pg.bg, c)
                sel = R(n * (c["count"] - 1)) == c["index"]
                img.alpha_composite(on if sel else off, (x, y))
            elif c["kind"] == "key":
                off, on = key_pics(pg.bg, c)
                img.alpha_composite(on if PBYKEY[key].get("default", 0) and PBYKEY[key].get("options") else off, (x, y))
            elif c["kind"] == "value":
                p = PBYKEY[key]
                s = "INIT" if key == "patch_name" else (p["options"][p["default"]] if p.get("options") else str(p["default"]))
                f = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", R(c.get("px", 21) * 0.72))
                if c.get("left"):
                    d.text((x + 10, y + h / 2), s, font=f, fill="#" + LCD_INK, anchor="lm")
                else:
                    d.text((x + w / 2, y + h / 2), s, font=f, fill="#" + LCD_INK, anchor="mm")
        img.convert("RGB").save(os.path.join(out_dir, "page_%d_%s.png" % (tab, TABS[tab].replace("/", "_").replace(" ", "_"))))
    print("preview: %s" % out_dir)


if __name__ == "__main__":
    if "--layout" in sys.argv:
        open(os.path.join(ROOT, "layout.conf"), "w").write(layout_conf())
        print("layout.conf written")
    elif "--check" in sys.argv:
        if open(os.path.join(ROOT, "layout.conf")).read() != layout_conf():
            raise SystemExit("layout.conf is stale: run python3 tools/skin.py --layout")
    elif "--preview" in sys.argv:
        preview(sys.argv[sys.argv.index("--preview") + 1])
    else:
        build(sys.argv[1])
