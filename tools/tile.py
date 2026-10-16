#!/usr/bin/env python3
"""GAIAXXL's tile in MPC's Sounds > INSTRUMENTS browser (270 x 110): a strip of the TONE page with the name."""
import os
import sys
from PIL import Image, ImageDraw, ImageEnhance

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import skin  # noqa: E402

pg = skin.tone_page(0)
img = pg.bg.crop((386, 300, 386 + 540, 300 + 220)).resize((270, 110), Image.LANCZOS)
img = ImageEnhance.Brightness(img).enhance(0.45)
d = ImageDraw.Draw(img)
d.rounded_rectangle((8, 22, 262, 88), radius=10, fill=(20, 18, 18), outline=skin.AMBER, width=2)
d.text((135, 48), "GAIAXXL", font=skin.font(40), fill=skin.AMBER, anchor="mm")
d.text((135, 76), "3-TONE VIRTUAL ANALOG", font=skin.font(16), fill=skin.INK, anchor="mm")
img.save(os.path.join(skin.ROOT, "art", "tile.png"))
print("art/tile.png")
