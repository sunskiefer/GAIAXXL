"""GAIAXXL caps, top view, rendered from scratch after the user's reference photos (generic hardware parts):
knob_pointer  = black fluted knob, brushed-aluminium top with a black pointer line (parameters)
knob_endless  = the same knob, aluminium top with no line (data / endless)
slider_white / slider_black / slider_grey = tapered fader caps seen from above, a centre line in the contrast colour"""
import math, os, numpy as np
from PIL import Image, ImageDraw, ImageFilter
OUT = os.path.join(os.path.dirname(__file__), "..", "art", "caps")
SS = 4
PANEL = (90, 86, 85)

def shadow(S_w, S_h, box, r, blur):
    m = Image.new("L", (S_w, S_h), 0); ImageDraw.Draw(m).rounded_rectangle(box, radius=r, fill=160)
    return m.filter(ImageFilter.GaussianBlur(blur))

def knob(px, pointer):
    S = px * SS; c = S / 2
    y, x = np.mgrid[0:S, 0:S].astype(np.float32)
    dx, dy = x - c, y - c
    rad, ang = np.hypot(dx, dy), np.arctan2(dy, dx)
    img = np.zeros((S, S, 4), np.float32)
    # contact shadow
    sh = np.asarray(shadow(S, S, (S*.06, S*.10, S*.98, S*1.0), S*.46, S*.035), np.float32) / 255
    img[..., 3] = sh * 200
    R_base, R_skirt, R_top = S*.46, S*.40, S*.265
    # bell base ring (the flare seen around the body), lit from top-left
    light = np.cos(ang + 2.35)                     # +1 toward top-left
    base = (rad <= R_base)
    col = 26 + 14 * light
    # flutes on the body: 24 ridges
    flute = 0.5 + 0.5 * np.cos(ang * 24)
    body = (rad <= R_skirt)
    col = np.where(body, 30 + 16 * light + 10 * flute * (0.6 + 0.4 * light), col)
    # dark bevel ring between body and the aluminium top
    bevel = (rad <= R_top * 1.10)
    col = np.where(bevel, 14, col)
    for ch in range(3): img[..., ch] = np.where(base, col, img[..., ch])
    img[..., 3] = np.where(base, 255, img[..., 3])
    # aluminium top: brushed (radial streaks) + spin-lathe conic highlights, knurled edge
    rng = np.random.default_rng(7)
    streak = np.interp((ang + math.pi) / (2*math.pi) * 2048 % 2048, np.arange(2048), rng.normal(0, 1, 2048))
    conic = 0.5 + 0.5 * np.cos(2 * (ang + 0.75))  # two bright lobes, like turned aluminium
    al = 150 + 70 * conic + 9 * streak - 25 * (rad / R_top)
    edge_r = R_top * (1 - 0.035 * (0.5 + 0.5 * np.cos(ang * 40)))   # serrated rim
    top = rad <= edge_r
    for ch, t in enumerate((1.0, 1.0, 1.03)):
        img[..., ch] = np.where(top, np.clip(al * t, 0, 255), img[..., ch])
    rim = (rad <= edge_r) & (rad > edge_r - S*.012)
    for ch in range(3): img[..., ch] = np.where(rim, img[..., ch] * 0.6, img[..., ch])
    out = Image.fromarray(np.clip(img, 0, 255).astype(np.uint8), "RGBA")
    if pointer:   # black engraved line from near the centre to the rim, pointing up (rotated in the UI)
        d = ImageDraw.Draw(out); w = S * .028
        d.rounded_rectangle((c - w/2, c - R_top*.97, c + w/2, c - R_top*.28), radius=w/2, fill=(10, 10, 10, 255))
    return out.resize((px, px), Image.LANCZOS)

def slider(w, h, body, line):
    """top view of a tapered fader cap: the wide flared foot around a narrower flat top with a groove + line"""
    W, H = w * SS, h * SS
    out = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    out.putalpha(shadow(W, H, (W*.06, H*.07, W*.99, H*.99), W*.08, W*.04))
    blk = Image.new("RGBA", (W, H), (0, 0, 0, 255)); blk.putalpha(out.getchannel("A")); out = blk
    b = np.array(body, np.float32)
    d = ImageDraw.Draw(out)
    # foot (flare), slightly darker, lit edge top-left
    d.rounded_rectangle((W*.03, H*.03, W*.93, H*.93), radius=W*.06, fill=tuple(int(v) for v in b * .72) + (255,))
    # the sloped sides between foot and top: darker toward bottom/right
    for i, f in enumerate(np.linspace(0, 1, 14)):
        inset = f
        x0, y0 = W*(.03 + .12*inset), H*(.03 + .10*inset)
        x1, y1 = W*(.93 - .12*inset), H*(.93 - .10*inset)
        k = .74 + .14 * f
        d.rounded_rectangle((x0, y0, x1, y1), radius=W*.05, fill=tuple(int(min(255, v*k)) for v in b) + (255,))
    # flat top face
    tx0, ty0, tx1, ty1 = W*.15, H*.13, W*.81, H*.83
    d.rounded_rectangle((tx0, ty0, tx1, ty1), radius=W*.04, fill=tuple(int(v) for v in b) + (255,))
    d.line((tx0, ty0, tx1, ty0), fill=tuple(int(min(255, v*1.12 + 8)) for v in b) + (255,), width=SS)   # lit top edge
    # groove across the middle + the indicator line in it
    gy = (ty0 + ty1) / 2
    d.rectangle((tx0, gy - H*.07, tx1, gy + H*.07), fill=tuple(int(v*.82) for v in b) + (255,))
    d.rectangle((tx0, gy - H*.022, tx1, gy + H*.022), fill=line + (255,))
    return out.resize((w, h), Image.LANCZOS)

if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    caps = {"knob_pointer": knob(96, True), "knob_endless": knob(96, False),
            "slider_white": slider(48, 64, (236, 234, 228), (25, 25, 25)),
            "slider_black": slider(48, 64, (34, 33, 33), (245, 245, 245)),
            "slider_grey":  slider(48, 64, (172, 172, 170), (25, 25, 25))}
    for k, im in caps.items(): im.save(os.path.join(OUT, k + ".png"))
    for old in ("slider_amber", "slider_blue"):
        p = os.path.join(OUT, old + ".png")
        if os.path.exists(p): os.remove(p)
    sheet = Image.new("RGBA", (560, 150), PANEL + (255,))
    x = 22
    for k in ("knob_pointer", "knob_endless"):
        sheet.alpha_composite(caps[k], (x, 27)); x += 118
    for k in ("slider_white", "slider_black", "slider_grey"):
        sheet.alpha_composite(caps[k], (x + 14, 43)); x += 92
    sheet.resize((1680, 450), Image.LANCZOS).save(os.path.join(OUT, "caps_preview.png"))
