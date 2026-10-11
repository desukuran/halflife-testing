#!/usr/bin/env python3
"""
Zelda 64 port: builds the HUD sprites (.spr, Half-Life sprite v2) from scratch.

Everything here is drawn by this script: no ROM data. When you rip the real OoT
textures with tools/zelda/rip_textures.py, they are written over these files with the
same frame layout, so the HUD code doesn't care which set is installed.

Usage:
    python3 make_hud_sprites.py [--out ../../zelda/sprites] [--font path/to/font.ttf]

Needs Pillow (pip install pillow).

Frame layouts must match cl_dll/zl_hud.h.
"""

import argparse
import math
import os
import struct
import sys

from PIL import Image, ImageDraw, ImageFont, ImageFilter

SPR_VP_PARALLEL = 2
SPR_NORMAL = 0
SPR_ADDITIVE = 1
SPR_INDEXALPHA = 2
SPR_ALPHTEST = 3

SS = 4  # supersampling factor for smooth edges


# ---------------------------------------------------------------------------
# .spr writer
# ---------------------------------------------------------------------------

def write_spr(path, frames, tex_format, palette):
    """frames: list of (width, height, bytes) in palette indices."""
    maxw = max(f[0] for f in frames)
    maxh = max(f[1] for f in frames)
    radius = math.sqrt((maxw / 2) ** 2 + (maxh / 2) ** 2)

    out = bytearray()
    out += b"IDSP"
    out += struct.pack("<ii", 2, SPR_VP_PARALLEL)
    out += struct.pack("<i", tex_format)
    out += struct.pack("<f", radius)
    out += struct.pack("<iii", maxw, maxh, len(frames))
    out += struct.pack("<f", 0.0)       # beamlength
    out += struct.pack("<i", 0)         # synctype
    out += struct.pack("<h", 256)
    for r, g, b in palette:
        out += bytes((r, g, b))
    for w, h, data in frames:
        out += struct.pack("<i", 0)     # SPR_SINGLE
        out += struct.pack("<iiii", -(w // 2), h // 2, w, h)
        assert len(data) == w * h
        out += data

    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(out)
    print(f"wrote {path} ({len(frames)} frames)")


def indexalpha_palette():
    # INDEXALPHA: the index is the alpha, the colour is palette[255]
    return [(i, i, i) for i in range(255)] + [(255, 255, 255)]


def alpha_frame(img):
    """Greyscale 'L' image -> INDEXALPHA frame bytes."""
    return (img.width, img.height, img.tobytes())


def render_l(size, draw_fn):
    """Draw at SS x size with white on black, downsample to an L image."""
    w, h = size
    big = Image.new("L", (w * SS, h * SS), 0)
    d = ImageDraw.Draw(big)
    draw_fn(d, SS)
    return big.resize((w, h), Image.LANCZOS)


def rgba_frames_to_alphtest(images):
    """RGBA images -> shared 255 colour palette, index 255 transparent."""
    w, h = images[0].size
    sheet = Image.new("RGBA", (w, h * len(images)), (0, 0, 0, 0))
    for i, im in enumerate(images):
        sheet.paste(im, (0, i * h))

    rgb = Image.new("RGB", sheet.size, (0, 0, 0))
    rgb.paste(sheet, mask=sheet.split()[3])
    q = rgb.quantize(colors=255, method=Image.MEDIANCUT)
    pal = q.getpalette()[: 255 * 3]
    pal += [0] * (255 * 3 - len(pal))
    palette = [tuple(pal[i * 3: i * 3 + 3]) for i in range(255)] + [(0, 0, 255)]

    alpha = sheet.split()[3]
    idx = bytearray(q.tobytes())
    a = alpha.tobytes()
    for i in range(len(idx)):
        if a[i] < 128:
            idx[i] = 255

    frames = []
    for i in range(len(images)):
        start = i * w * h
        frames.append((w, h, bytes(idx[start: start + w * h])))
    return frames, palette


# ---------------------------------------------------------------------------
# Shapes
# ---------------------------------------------------------------------------

def heart_polygon(cx, cy, s, steps=64):
    """Classic heart curve, s = half width."""
    pts = []
    for i in range(steps):
        t = 2 * math.pi * i / steps
        x = 16 * math.sin(t) ** 3
        y = 13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t)
        pts.append((cx + x * s / 17.0, cy - y * s / 17.0))
    return pts


def make_hearts(out):
    size = 32
    frames = []

    def fill(fraction):
        def fn(d, k):
            c = size * k / 2
            d.polygon(heart_polygon(c, c + 1 * k, 12.5 * k), fill=255)
        img = render_l((size, size), fn)
        if fraction < 1.0:
            # OoT fills quarters going round from the bottom left: 1/4, 1/2 (left), 3/4
            mask = Image.new("L", (size, size), 0)
            md = ImageDraw.Draw(mask)
            h = size // 2
            if fraction >= 0.25:
                md.rectangle((0, h, h - 1, size), fill=255)
            if fraction >= 0.5:
                md.rectangle((0, 0, h - 1, h - 1), fill=255)
            if fraction >= 0.75:
                md.rectangle((h, h, size, size), fill=255)
            img = Image.composite(img, Image.new("L", img.size, 0), mask)
        return img

    for f in (1.0, 0.75, 0.5, 0.25):
        frames.append(alpha_frame(fill(f)))

    def outline(d, k):
        c = size * k / 2
        d.polygon(heart_polygon(c, c + 1 * k, 15.5 * k), fill=255)
        d.polygon(heart_polygon(c, c + 1 * k, 12.5 * k), fill=0)
    frames.append(alpha_frame(render_l((size, size), outline)))

    write_spr(os.path.join(out, "zl_heart.spr"), frames, SPR_INDEXALPHA, indexalpha_palette())


def make_white(out):
    frames = [(8, 8, bytes([255] * 64))]
    write_spr(os.path.join(out, "zl_white.spr"), frames, SPR_INDEXALPHA, indexalpha_palette())


def make_buttons(out):
    frames = []
    s = 64

    # 0 filled circle
    frames.append(alpha_frame(render_l((s, s), lambda d, k: d.ellipse((1 * k, 1 * k, (s - 1) * k, (s - 1) * k), fill=255))))

    # 1 ring
    def ring(d, k):
        d.ellipse((1 * k, 1 * k, (s - 1) * k, (s - 1) * k), fill=255)
        d.ellipse((6 * k, 6 * k, (s - 6) * k, (s - 6) * k), fill=0)
    frames.append(alpha_frame(render_l((s, s), ring)))

    # 2 C button arrow (points up; the HUD rotates it per button)
    def arrow(d, k):
        d.polygon([(32 * k, 12 * k), (50 * k, 44 * k), (14 * k, 44 * k)], fill=255)
    frames.append(alpha_frame(render_l((s, s), arrow)))

    write_spr(os.path.join(out, "zl_button.spr"), frames, SPR_INDEXALPHA, indexalpha_palette())


def make_reticle(out):
    frames = []
    s = 32

    # 0 lock-on triangle, points down at the target (z_actor.c Attention_Draw reticle)
    frames.append(alpha_frame(render_l((s, s), lambda d, k: d.polygon(
        [(3 * k, 6 * k), (29 * k, 6 * k), (16 * k, 28 * k)], fill=255))))

    # 1 Navi arrow over the attention actor
    def navi(d, k):
        d.polygon([(4 * k, 4 * k), (28 * k, 4 * k), (16 * k, 28 * k)], fill=255)
        d.polygon([(10 * k, 8 * k), (22 * k, 8 * k), (16 * k, 19 * k)], fill=150)
    frames.append(alpha_frame(render_l((s, s), navi)))

    write_spr(os.path.join(out, "zl_reticle.spr"), frames, SPR_INDEXALPHA, indexalpha_palette())


def make_textbox(out):
    frames = []
    s = 16

    # 0 top left rounded corner for the message box
    frames.append(alpha_frame(render_l((s, s), lambda d, k: d.ellipse((0, 0, 32 * k, 32 * k), fill=255))))
    # 1 "next box" triangle (points down)
    frames.append(alpha_frame(render_l((s, s), lambda d, k: d.polygon([(2 * k, 4 * k), (14 * k, 4 * k), (8 * k, 13 * k)], fill=255))))
    # 2 "end" square
    frames.append(alpha_frame(render_l((s, s), lambda d, k: d.rectangle((3 * k, 3 * k, 13 * k, 13 * k), fill=255))))
    # 3 choice cursor (points right)
    frames.append(alpha_frame(render_l((s, s), lambda d, k: d.polygon([(3 * k, 2 * k), (14 * k, 8 * k), (3 * k, 14 * k)], fill=255))))

    write_spr(os.path.join(out, "zl_textbox.spr"), frames, SPR_INDEXALPHA, indexalpha_palette())


def make_hud_icons(out):
    """Tinted single colour icons: rupee, small key (gRupeeCounterIconTex, gSmallKeyCounterIconTex)."""
    frames = []
    s = 32

    def rupee(d, k):
        pts = [(16, 2), (25, 9), (25, 23), (16, 30), (7, 23), (7, 9)]
        d.polygon([(x * k, y * k) for x, y in pts], fill=255)
        inner = [(16, 8), (20, 11), (20, 21), (16, 24), (12, 21), (12, 11)]
        d.polygon([(x * k, y * k) for x, y in inner], fill=170)
    frames.append(alpha_frame(render_l((s, s), rupee)))

    def key(d, k):
        d.ellipse((8 * k, 2 * k, 24 * k, 16 * k), fill=255)
        d.ellipse((12 * k, 6 * k, 20 * k, 12 * k), fill=0)
        d.rectangle((14 * k, 14 * k, 18 * k, 30 * k), fill=255)
        d.rectangle((18 * k, 22 * k, 23 * k, 25 * k), fill=255)
        d.rectangle((18 * k, 27 * k, 22 * k, 30 * k), fill=255)
    frames.append(alpha_frame(render_l((s, s), key)))

    # Frames 2-11: counter digits 0-9 (gCounterDigit0Tex .. 9, 8x16 in OoT, here 16x32)
    path = find_font(None)
    font = ImageFont.truetype(path, 30 * SS) if path else ImageFont.load_default(size=30 * SS)
    for digit in "0123456789":
        def draw_digit(d, k, digit=digit):
            box = d.textbbox((0, 0), digit, font=font)
            x = (16 * k - (box[2] - box[0])) / 2 - box[0]
            y = (32 * k - (box[3] - box[1])) / 2 - box[1]
            d.text((x, y), digit, font=font, fill=255)
        frames.append(alpha_frame(render_l((16, 32), draw_digit)))

    write_spr(os.path.join(out, "zl_hudicons.spr"), frames, SPR_INDEXALPHA, indexalpha_palette())


# ---------------------------------------------------------------------------
# Item icons (icon_item_static in OoT). Full colour, alpha tested.
# Order must match ZICON_* in cl_dll/zl_hud.h
# ---------------------------------------------------------------------------

def icon_canvas():
    return Image.new("RGBA", (32 * SS, 32 * SS), (0, 0, 0, 0))


def finish_icon(img):
    return img.resize((32, 32), Image.LANCZOS)


def P(*pts):
    return [(x * SS, y * SS) for x, y in pts]


def icon_sword(blade, hilt, grip, length):
    img = icon_canvas()
    d = ImageDraw.Draw(img)
    # diagonal: tip top right, handle bottom left
    tip = (29, 3)
    guard = (29 - length, 3 + length)
    d.polygon(P((tip[0], tip[1]), (guard[0] + 3, guard[1] + 1), (guard[0] - 1, guard[1] - 3)), fill=blade)
    d.line(P((guard[0] - 4, guard[1] - 4), (guard[0] + 4, guard[1] + 4)), fill=hilt, width=4 * SS)
    d.line(P(guard, (guard[0] - 6, guard[1] + 6)), fill=grip, width=3 * SS)
    d.ellipse(P((guard[0] - 9, guard[1] + 5), (guard[0] - 5, guard[1] + 9)), fill=hilt)
    return finish_icon(img)


def icon_bow():
    img = icon_canvas()
    d = ImageDraw.Draw(img)
    d.arc(P((4, 2), (26, 30)), start=250, end=110, fill=(150, 95, 40, 255), width=4 * SS)
    d.line(P((12, 4), (12, 28)), fill=(235, 235, 220, 255), width=1 * SS)
    d.line(P((3, 16), (28, 16)), fill=(200, 180, 140, 255), width=2 * SS)
    d.polygon(P((28, 16), (24, 13), (24, 19)), fill=(200, 200, 210, 255))
    d.polygon(P((3, 16), (6, 13), (6, 19)), fill=(220, 60, 60, 255))
    return finish_icon(img)


def icon_bomb():
    img = icon_canvas()
    d = ImageDraw.Draw(img)
    d.ellipse(P((4, 8), (26, 30)), fill=(40, 50, 120, 255))
    d.ellipse(P((8, 12), (14, 18)), fill=(120, 140, 230, 255))
    d.rectangle(P((13, 5), (18, 10)), fill=(160, 160, 170, 255))
    d.line(P((16, 5), (22, 1)), fill=(230, 200, 120, 255), width=2 * SS)
    d.ellipse(P((21, 0), (25, 4)), fill=(255, 140, 20, 255))
    return finish_icon(img)


def icon_hookshot(long_shot):
    img = icon_canvas()
    d = ImageDraw.Draw(img)
    body = (120, 120, 200, 255) if long_shot else (90, 110, 160, 255)
    d.rectangle(P((4, 14), (20, 20)), fill=body)
    d.rectangle(P((6, 20), (11, 28)), fill=(110, 70, 40, 255))
    d.polygon(P((20, 12), (29, 17), (20, 22)), fill=(210, 210, 220, 255))
    d.line(P((22, 13), (26, 9)), fill=(210, 210, 220, 255), width=2 * SS)
    d.line(P((22, 21), (26, 25)), fill=(210, 210, 220, 255), width=2 * SS)
    if long_shot:
        d.rectangle(P((2, 15), (5, 19)), fill=(230, 210, 80, 255))
    return finish_icon(img)


def icon_nut():
    img = icon_canvas()
    d = ImageDraw.Draw(img)
    d.ellipse(P((6, 6), (26, 28)), fill=(160, 110, 50, 255))
    d.ellipse(P((10, 9), (17, 15)), fill=(210, 170, 100, 255))
    d.line(P((16, 6), (16, 28)), fill=(110, 70, 30, 255), width=1 * SS)
    return finish_icon(img)


def icon_stick():
    img = icon_canvas()
    d = ImageDraw.Draw(img)
    d.line(P((5, 28), (27, 4)), fill=(150, 100, 50, 255), width=4 * SS)
    d.line(P((5, 28), (27, 4)), fill=(190, 140, 80, 255), width=1 * SS)
    return finish_icon(img)


def icon_bottle():
    img = icon_canvas()
    d = ImageDraw.Draw(img)
    d.rectangle(P((12, 2), (20, 6)), fill=(170, 120, 70, 255))
    d.polygon(P((13, 6), (19, 6), (25, 14), (25, 29), (7, 29), (7, 14)), fill=(190, 220, 240, 170))
    d.line(P((10, 14), (10, 26)), fill=(255, 255, 255, 220), width=2 * SS)
    return finish_icon(img)


def icon_pistol():
    img = icon_canvas()
    d = ImageDraw.Draw(img)
    d.rectangle(P((4, 10), (27, 16)), fill=(70, 70, 80, 255))
    d.polygon(P((6, 16), (13, 16), (11, 28), (5, 28)), fill=(50, 50, 55, 255))
    d.rectangle(P((14, 16), (17, 20)), fill=(40, 40, 45, 255))
    d.rectangle(P((24, 8), (26, 10)), fill=(200, 140, 0, 255))
    return finish_icon(img)


def icon_rupee():
    img = icon_canvas()
    d = ImageDraw.Draw(img)
    d.polygon(P((16, 2), (25, 9), (25, 23), (16, 30), (7, 23), (7, 9)), fill=(30, 190, 60, 255))
    d.polygon(P((16, 8), (20, 11), (20, 21), (16, 24), (12, 21), (12, 11)), fill=(120, 255, 140, 255))
    return finish_icon(img)


def icon_key():
    img = icon_canvas()
    d = ImageDraw.Draw(img)
    c = (200, 200, 220, 255)
    d.ellipse(P((8, 2), (24, 16)), fill=c)
    d.ellipse(P((12, 6), (20, 12)), fill=(0, 0, 0, 0))
    d.rectangle(P((14, 14), (18, 30)), fill=c)
    d.rectangle(P((18, 22), (23, 25)), fill=c)
    return finish_icon(img)


def icon_arrow():
    img = icon_canvas()
    d = ImageDraw.Draw(img)
    d.line(P((4, 28), (26, 6)), fill=(200, 170, 120, 255), width=2 * SS)
    d.polygon(P((28, 4), (20, 7), (25, 12)), fill=(200, 200, 210, 255))
    d.polygon(P((4, 28), (4, 22), (10, 28)), fill=(220, 60, 60, 255))
    return finish_icon(img)


def icon_heart_container():
    img = icon_canvas()
    d = ImageDraw.Draw(img)
    d.polygon([(x, y) for x, y in heart_polygon(16 * SS, 17 * SS, 14 * SS)], fill=(255, 70, 50, 255))
    d.polygon([(x, y) for x, y in heart_polygon(13 * SS, 13 * SS, 4 * SS)], fill=(255, 190, 180, 255))
    return finish_icon(img)


def icon_shield():
    img = icon_canvas()
    d = ImageDraw.Draw(img)
    d.polygon(P((6, 4), (26, 4), (26, 18), (16, 30), (6, 18)), fill=(60, 80, 170, 255))
    d.polygon(P((9, 7), (23, 7), (23, 17), (16, 26), (9, 17)), fill=(150, 160, 190, 255))
    d.polygon(P((16, 10), (20, 17), (12, 17)), fill=(230, 200, 60, 255))
    d.polygon(P((16, 22), (20, 15), (12, 15)), fill=(200, 40, 40, 255))
    return finish_icon(img)


def make_item_icons(out):
    icons = [
        icon_sword((210, 210, 220, 255), (90, 140, 70, 255), (120, 80, 40, 255), 16),   # 0 ZICON_SWORD_KOKIRI
        icon_sword((230, 235, 255, 255), (70, 70, 200, 255), (90, 60, 140, 255), 20),   # 1 ZICON_SWORD_MASTER
        icon_sword((230, 230, 240, 255), (200, 160, 60, 255), (120, 80, 40, 255), 24),  # 2 ZICON_SWORD_BIGGORON
        icon_bow(),                     # 3 ZICON_BOW
        icon_bomb(),                    # 4 ZICON_BOMB
        icon_hookshot(False),           # 5 ZICON_HOOKSHOT
        icon_hookshot(True),            # 6 ZICON_LONGSHOT
        icon_nut(),                     # 7 ZICON_DEKU_NUT
        icon_stick(),                   # 8 ZICON_DEKU_STICK
        icon_bottle(),                  # 9 ZICON_BOTTLE
        icon_pistol(),                  # 10 ZICON_HLWEAPON
        icon_rupee(),                   # 11 ZICON_RUPEE
        icon_key(),                     # 12 ZICON_KEY
        icon_arrow(),                   # 13 ZICON_ARROW
        icon_heart_container(),         # 14 ZICON_HEART_CONTAINER
        icon_shield(),                  # 15 ZICON_SHIELD
    ]
    frames, palette = rgba_frames_to_alphtest(icons)
    write_spr(os.path.join(out, "zl_icons.spr"), frames, SPR_ALPHTEST, palette)


# ---------------------------------------------------------------------------
# Font (nes_font_static in OoT). One frame per ASCII character 32..126.
# ---------------------------------------------------------------------------

def find_font(user):
    if user:
        return user
    for p in (
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "C:/Windows/Fonts/arialbd.ttf",
        "/Library/Fonts/Arial Bold.ttf",
    ):
        if os.path.exists(p):
            return p
    return None


def make_font(out, font_path, header_path):
    cell = 32
    path = find_font(font_path)
    if path:
        font = ImageFont.truetype(path, 24)
        print(f"font: {path}")
    else:
        font = ImageFont.load_default(size=24)
        print("font: Pillow default (Aileron, CC0)")

    frames = []
    widths = []
    ascent, descent = font.getmetrics()
    baseline = int((cell - (ascent + descent)) / 2 + ascent)

    for c in range(32, 127):
        ch = chr(c)
        img = Image.new("L", (cell, cell), 0)
        d = ImageDraw.Draw(img)
        adv = font.getlength(ch)
        x = max(0, int((cell - adv) / 2))
        d.text((x, baseline), ch, fill=255, font=font, anchor="ls")
        frames.append(alpha_frame(img))
        widths.append(int(round(adv)) if ch != " " else int(cell * 0.3))

    write_spr(os.path.join(out, "zl_font.spr"), frames, SPR_INDEXALPHA, indexalpha_palette())

    # The client reads this when present (a ripped font writes its own); keep it in step
    with open(os.path.join(out, "zl_font.txt"), "w") as f:
        f.write("// Advance widths for zl_font.spr (32 px cells), ASCII 32..126\n")
        f.write(" ".join(str(w) for w in widths) + "\n")

    if header_path:
        with open(header_path, "w") as f:
            f.write("// Generated by tools/zelda/make_hud_sprites.py, do not edit.\n")
            f.write("// Advance width of each glyph in sprites/zl_font.spr (32 px cells), chars 32..126.\n")
            f.write("#ifndef ZL_FONTWIDTHS_H\n#define ZL_FONTWIDTHS_H\n\n")
            f.write("#define ZL_FONT_CELL %d\n\n" % cell)
            f.write("static const unsigned char g_zlFontWidths[95] =\n{\n")
            for i in range(0, len(widths), 16):
                f.write("\t" + ", ".join(str(w) for w in widths[i:i + 16]) + ",\n")
            f.write("};\n\n#endif\n")
        print(f"wrote {header_path}")


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(here, "..", "..", "zelda", "sprites"))
    ap.add_argument("--font", default=None)
    ap.add_argument("--header", default=os.path.join(here, "..", "..", "cl_dll", "zl_fontwidths.h"))
    args = ap.parse_args()

    out = os.path.normpath(args.out)
    make_white(out)
    make_hearts(out)
    make_buttons(out)
    make_reticle(out)
    make_textbox(out)
    make_hud_icons(out)
    make_item_icons(out)
    make_font(out, args.font, os.path.normpath(args.header))


if __name__ == "__main__":
    main()
