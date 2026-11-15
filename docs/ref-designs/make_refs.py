#!/usr/bin/env python3
"""Regenerates design-OS.png and design-system.png from the real OS (simulator renders) and palette.

Run from the repository root after building the simulator:
    cmake -S sim -B sim/build && cmake --build sim/build
    python3 docs/ref-designs/make_refs.py        (needs Pillow)
The simulator shows exact colours; the board applies its own colour correction (hardware.yaml).
"""
import os, re, subprocess, tempfile
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SIM = os.path.join(ROOT, "sim", "build", "tinygame-sim")
FONT = os.path.join(ROOT, "components", "tinygame", "fonts", "SpaceGrotesk.ttf")
CARTS = ["console/carts/brick.lua", "console/carts/examples/rocks.lua", "console/carts/examples/snake.lua",
         "console/carts/examples/tilt.lua", "console/carts/examples/paint.lua", "console/carts/examples/walkwalk.lua"]
SHEET, NAVY, BLUE = (208, 220, 255), (10, 20, 80), (10, 46, 184)

def font(size, weight=700):
    f = ImageFont.truetype(FONT, size)
    f.set_variation_by_axes([weight])
    return f

def shot(tmp, name, frames, args):
    out = os.path.join(tmp, name + ".png")
    subprocess.run([SIM, "--shell", *CARTS, "--frames", str(frames), *args, "--png", out, "--scale", "2"],
                   cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
    return Image.open(out).convert("RGB")

def palette():
    src = open(os.path.join(ROOT, "components/tinygame/gfx.c")).read()
    game = re.search(r"GAME\[32\] = \{(.*?)\};", src, re.S).group(1)
    ospal = re.search(r"OSPAL\[11\] = \{(.*?)\};", src, re.S).group(1)
    return ([int(h, 16) for h in re.findall(r"0x([0-9A-Fa-f]{6})", game)],
            [int(h, 16) for h in re.findall(r"0x([0-9A-Fa-f]{6})", ospal)])

def rgb(v): return ((v >> 16) & 255, (v >> 8) & 255, v & 255)

def screens():
    with tempfile.TemporaryDirectory() as tmp:
        shots = [
            ("01 / Home", shot(tmp, "home", 5, [])),
            ("02 / Game shelf", shot(tmp, "shelf", 12, ["--press", "right@3", "--turn", "right@8:2"])),
            ("03 / Game card", shot(tmp, "card", 20, ["--press", "right@3", "--turn", "right@8:1", "--press", "right@12"])),
            ("04 / Settings", shot(tmp, "settings", 20, ["--turn", "right@3:2", "--press", "right@8"])),
            ("05 / Paused", shot(tmp, "paused", 40, ["--press", "right@3", "--press", "right@6", "--press", "right@9", "--chord", "pause@25"])),
            ("06 / Light", shot(tmp, "light", 20, ["--turn", "right@3:2", "--press", "right@8", "--press", "right@12"])),
        ]
    w, h = 640, 344
    pad, gap, lab, top = 32, 28, 26, 70
    W = pad * 2 + 2 * w + gap
    H = top + pad + 3 * (h + lab + gap)
    im = Image.new("RGB", (W, H), SHEET)
    d = ImageDraw.Draw(im)
    d.text((pad, pad - 4), "TinyGame OS / Deep Toy Blue", font=font(36), fill=NAVY)
    for i, (title, img) in enumerate(shots):
        x = pad + (i % 2) * (w + gap)
        y = top + (i // 2) * (h + lab + gap)
        d.text((x, y), title.upper(), font=font(18, 600), fill=NAVY)
        im.paste(img.resize((w, h)), (x, y + lab))
    note = font(15, 500)
    d.text((pad, H - 30), "Simulator renders show exact colours. The board applies a colour correction (hardware.yaml): judge colour on the board.",
           font=note, fill=NAVY)
    im.save(os.path.join(ROOT, "docs/ref-designs/design-OS.png"))

def system():
    game, ospal = palette()
    names = ["black", "navy", "dark grey", "grey", "white", "cream", "coral", "red", "yellow", "orange", "green", "teal",
             "mint", "blue", "pink", "brown", "bright red", "bright orange", "lemon", "lime", "bright green", "turquoise",
             "sky", "bright blue", "deep blue", "violet", "magenta", "hot pink", "bubblegum", "pale pink", "peach", "plum"]
    osnames = ["ink", "white", "red", "purple", "yellow", "green", "field blue", "orange", "muted", "pale blue", "royal"]
    W, H = 1400, 1180
    im = Image.new("RGB", (W, H), SHEET)
    d = ImageDraw.Draw(im)
    d.text((40, 28), "TinyGame OS / Design system", font=font(40), fill=NAVY)
    d.text((40, 84), "Deep Toy Blue. Flat fills, white type on blue, yellow action blocks, drop-edges 45 degrees down-right in a darker tone of the block.",
           font=font(19, 500), fill=NAVY)
    d.text((40, 130), "OS COLOURS (SLOTS 64-74)", font=font(17, 600), fill=NAVY)
    for i, v in enumerate(ospal):
        x = 40 + i * 122
        d.rectangle((x, 160, x + 108, 214), fill=rgb(v), outline=NAVY, width=2)
        d.text((x, 220), osnames[i], font=font(14), fill=NAVY)
        d.text((x, 238), "#%06X" % v, font=font(12, 500), fill=NAVY)
    d.text((40, 280), "GAME COLOURS (0-31): 0-15 ORIGINAL SOFT SET, 16-31 BRIGHT AND PURE", font=font(17, 600), fill=NAVY)
    for i, v in enumerate(game):
        x = 40 + (i % 8) * 168
        y = 310 + (i // 8) * 82
        d.rectangle((x, y, x + 150, y + 44), fill=rgb(v), outline=NAVY, width=2)
        d.text((x, y + 48), "%d %s" % (i, names[i]), font=font(13), fill=NAVY)
    d.text((40, 650), "TYPE: SPACE GROTESK BOLD, 4 BITMAP SIZES (13, 16, 24, 32 PX)", font=font(17, 600), fill=NAVY)
    for i, (sz, t) in enumerate([(32, "START   XL, main button"), (24, "Games   L, titles and names"),
                                 (16, "Resume   M, list items and labels"), (13, "Select   S, hints, counts, taglines")]):
        d.text((40, 686 + i * 50), t, font=font(sz), fill=NAVY)
    d.text((40, 910), "COMPONENTS", font=font(17, 600), fill=NAVY)
    y = 946
    yellow, yellow_edge, pale, pale_edge = (255, 230, 0), (204, 184, 0), (208, 220, 255), (166, 176, 204)
    for x, fill, edge, label in ((40, yellow, yellow_edge, "Focused: yellow, navy text"), (320, pale, pale_edge, "Idle: pale blue")):
        d.rectangle((x + 4, y + 4, x + 194, y + 58), fill=edge)
        d.rectangle((x, y, x + 190, y + 54), fill=fill)
        d.text((x + 95, y + 27), "START", font=font(30), fill=NAVY, anchor="mm")
        d.text((x, y + 76), label, font=font(14, 500), fill=NAVY)
    x = 600
    for body, shadow in (((255, 35, 56), (204, 28, 45)), ((0, 222, 90), (0, 178, 72))):
        for off, col in ((4, shadow), (0, body)):
            X = x + off
            d.polygon([(X, y + off + 12), (X + 12, y + off), (X + 78, y + off), (X + 90, y + off + 12), (X + 90, y + off + 96), (X, y + off + 96)], fill=col)
        win = tuple(c + (255 - c) // 4 for c in body)
        d.rectangle((x + 8, y + 20, x + 82, y + 88), fill=win)
        x += 120
    d.text((600, y + 116), "Cartridge: body colour, window 25% brighter, shadow 20% darker", font=font(14, 500), fill=NAVY)
    d.text((1000, y), "List item", font=font(24), fill=NAVY)
    d.polygon([(970, y + 6), (982, y + 14), (970, y + 22)], fill=NAVY)
    d.rectangle((1000, y + 34, 1110, y + 37), fill=(255, 230, 0), outline=NAVY)
    d.text((1000, y + 76), "pointer and yellow underline when focused", font=font(14, 500), fill=NAVY)
    d.text((40, H - 56), "Board colour correction: R 0.40, G 0.60, B 0.60 (hardware.yaml), tuned on the Colour Card. Simulator colours are exact.", font=font(15, 500), fill=NAVY)
    d.text((40, H - 32), "Source of truth: components/tinygame/os.c, gfx.c, gfx.h, fonts/gen_font.py. Regenerate this image with docs/ref-designs/make_refs.py.", font=font(15, 500), fill=NAVY)
    im.save(os.path.join(ROOT, "docs/ref-designs/design-system.png"))

if __name__ == "__main__":
    screens()
    system()
    print("wrote design-OS.png and design-system.png")
