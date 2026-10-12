#!/usr/bin/env python3
"""
Zelda 64 port: pull OoT's own HUD art and text out of YOUR copy of the game, through the
zeldaret/oot decompilation's asset extractor.

Nothing this writes may be redistributed: it comes from your ROM. The mod's .gitignore
keeps it out of the repository.

1. Set up the decomp with your ROM (once):
       cd oot
       cp /path/to/your/rom.z64 baseroms/ntsc-1.0/baserom.z64
       make setup VERSION=ntsc-1.0
   (Any version the decomp supports works: ntsc-1.0, ntsc-1.2, gc-us, pal-1.0 ...)

2. Run this:
       python3 rip_from_decomp.py --oot ../../../oot --version ntsc-1.0

It writes, under zelda/ (all of it git ignored):
    sprites/oot/zl_heart.spr     gHeart*Tex (parameter_static)
    sprites/oot/zl_hudicons.spr  gRupeeCounterIconTex, gSmallKeyCounterIconTex, gCounterDigit0-9Tex
    sprites/oot/zl_magic.spr     gMagicMeterEndTex, gMagicMeterMidTex, gMagicMeterFillTex
    sprites/oot/zl_drops.spr     gDrop*Tex: the bombs, arrows, nuts and magic jars lying around
    sprites/oot/zl_button.spr    gButtonBackgroundTex, gEquippedItemOutlineTex, gEmptyCLeftArrowTex
    sprites/oot/zl_icons.spr     gItemIcon*Tex (icon_item_static), gQuestIcon*Tex
    sprites/oot/zl_font.spr      gMsgChar*Tex (nes_font_static) + zl_font.txt widths
    sprites/oot/zl_spinring_*.spr, zl_spincharge_*.spr, zl_hitmark.spr
                                 gSpinAttack*Tex, gEffHitMark*Tex (gameplay_keep), baked
                                 frame by frame with the colours OoT draws them in
    resource/zl_oot_messages.txt   every English message, in the zl_messages.txt format,
                                   named by text id (0x1000 ...), plus ITEM_* aliases

Frames that can't be found keep the generated art (run make_hud_sprites.py first).

Needs Pillow.
"""

import argparse
import os
import re
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_hud_sprites as mk  # noqa: E402


def find_png(assets, folder, name):
    d = os.path.join(assets, "textures", folder)
    if not os.path.isdir(d):
        return None
    for f in os.listdir(d):
        if f.startswith(name + ".") and f.endswith(".png"):
            return os.path.join(d, f)
    return None


def load_la(path):
    """Returns (intensity, alpha) images for an i/ia texture."""
    im = Image.open(path)
    if im.mode in ("LA", "La"):
        i, a = im.split()
    elif im.mode == "L":
        i = im
        a = im
    elif im.mode == "P":
        im = im.convert("RGBA")
        i = im.convert("L")
        a = im.split()[3]
    else:
        im = im.convert("RGBA")
        i = im.convert("L")
        a = im.split()[3]
    return i, a


def scale_nearest(img, size):
    return img.resize(size, Image.NEAREST)


def mul(a, b, fn=lambda x, y: x * y // 255):
    pa, pb = a.load(), b.load()
    out = Image.new("L", a.size)
    po = out.load()
    for y in range(a.size[1]):
        for x in range(a.size[0]):
            po[x, y] = fn(pa[x, y], pb[x, y])
    return out


def rip_hearts(assets, out):
    names = ["gHeartFullTex", "gHeartThreeQuarterTex", "gHeartHalfTex", "gHeartQuarterTex"]
    frames = []
    for n in names:
        p = find_png(assets, "parameter_static", n)
        if not p:
            print(f"  missing {n}, keeping generated hearts")
            return
        i, a = load_la(p)
        # Health_DrawMeter: colour = lerp(ENV, PRIM, I), so I picks the red fill
        fill = mul(i, a)
        frames.append(mk.alpha_frame(scale_nearest(fill, (32, 32))))
    p = find_png(assets, "parameter_static", "gHeartEmptyTex")
    if not p:
        print("  missing gHeartEmptyTex, keeping generated hearts")
        return
    i, a = load_la(p)
    outline = mul(i, a, lambda x, y: (255 - x) * y // 255 if y else 0)
    # Empty heart is mostly outline (ENV) plus a faint inside; take everything opaque
    outline = mul(Image.new("L", a.size, 255), a)
    frames.append(mk.alpha_frame(scale_nearest(outline, (32, 32))))
    mk.write_spr(os.path.join(out, "zl_heart.spr"), frames, mk.SPR_INDEXALPHA, mk.indexalpha_palette())


def rip_hud_icons(assets, out):
    frames = []
    for n in ("gRupeeCounterIconTex", "gSmallKeyCounterIconTex"):
        p = find_png(assets, "parameter_static", n)
        if not p:
            print(f"  missing {n}, keeping generated hud icons")
            return
        i, a = load_la(p)
        # Single tint: keep some of the shading in the alpha
        shaded = mul(i, a, lambda x, y: y * (90 + x * 165 // 255) // 255)
        frames.append(mk.alpha_frame(scale_nearest(shaded, (32, 32))))
    # Frames 2-11: the rupee / key counter digits (i8, the intensity is the coverage)
    for d in range(10):
        p = find_png(assets, "parameter_static", f"gCounterDigit{d}Tex")
        if not p:
            print(f"  missing gCounterDigit{d}Tex, keeping generated hud icons")
            return
        i, a = load_la(p)
        frames.append(mk.alpha_frame(scale_nearest(i, (16, 32))))
    mk.write_spr(os.path.join(out, "zl_hudicons.spr"), frames, mk.SPR_INDEXALPHA, mk.indexalpha_palette())


def rip_magic(assets, out):
    imgs = []
    for n in ("gMagicMeterEndTex", "gMagicMeterMidTex", "gMagicMeterFillTex"):
        p = find_png(assets, "parameter_static", n)
        if not p:
            print(f"  missing {n}, keeping the generated magic meter")
            return
        imgs.append(load_la(p))
    (end_i, end_a), (mid_i, mid_a), (fill_ia_i, fill_ia_a) = imgs
    # The fill is loaded as a 16x16 I4 texture (gDPLoadMultiBlock_4b); the extractor wrote the same
    # 64 bytes as 8x8 IA8. Each byte is two I4 texels: its I nibble, then its A nibble.
    fill_i = Image.new("L", (16, 16))
    pi, pa, po = fill_ia_i.load(), fill_ia_a.load(), fill_i.load()
    for y in range(8):
        for x in range(8):
            k = y * 8 + x
            hi, lo = round(pi[x, y] / 17), round(pa[x, y] / 17)
            po[(2 * k) % 16, (2 * k) // 16] = hi * 17
            po[(2 * k + 1) % 16, (2 * k + 1) // 16] = lo * 17
    mk.write_spr(os.path.join(out, "zl_magic.spr"), mk.magic_frames(end_i, end_a, mid_i, mid_a, fill_i),
                 mk.SPR_INDEXALPHA, mk.indexalpha_palette())


def rip_buttons(assets, out):
    frames = []
    for n, rot in (("gButtonBackgroundTex", 0), ("gEquippedItemOutlineTex", 0), ("gEmptyCLeftArrowTex", -90)):
        p = find_png(assets, "parameter_static", n)
        if not p:
            print(f"  missing {n}, keeping generated buttons")
            return
        i, a = load_la(p)
        img = mul(i, a)
        if rot:
            img = img.rotate(rot)   # the HUD expects the C arrow to point up
        frames.append(mk.alpha_frame(img.resize((64, 64), Image.LANCZOS)))
    mk.write_spr(os.path.join(out, "zl_button.spr"), frames, mk.SPR_INDEXALPHA, mk.indexalpha_palette())


ICONS = [
    ("icon_item_static", "gItemIconSwordKokiriTex"),
    ("icon_item_static", "gItemIconSwordMasterTex"),
    ("icon_item_static", "gItemIconSwordBiggoronTex"),
    ("icon_item_static", "gItemIconBowTex"),
    ("icon_item_static", "gItemIconBombTex"),
    ("icon_item_static", "gItemIconHookshotTex"),
    ("icon_item_static", "gItemIconLongshotTex"),
    ("icon_item_static", "gItemIconDekuNutTex"),
    ("icon_item_static", "gItemIconDekuStickTex"),
    ("icon_item_static", "gItemIconBottleEmptyTex"),
    (None, None),                                   # ZICON_HLWEAPON stays generated
    (None, None),                                   # ZICON_RUPEE
    ("icon_item_24_static", "gQuestIconSmallKeyTex"),
    (None, None),                                   # ZICON_ARROW
    ("icon_item_24_static", "gQuestIconHeartContainerTex"),
    ("icon_item_static", "gItemIconShieldHylianTex"),
]


def rip_item_icons(assets, out):
    generated = [
        mk.icon_sword((210, 210, 220, 255), (90, 140, 70, 255), (120, 80, 40, 255), 16),
        mk.icon_sword((230, 235, 255, 255), (70, 70, 200, 255), (90, 60, 140, 255), 20),
        mk.icon_sword((230, 230, 240, 255), (200, 160, 60, 255), (120, 80, 40, 255), 24),
        mk.icon_bow(), mk.icon_bomb(), mk.icon_hookshot(False), mk.icon_hookshot(True),
        mk.icon_nut(), mk.icon_stick(), mk.icon_bottle(), mk.icon_pistol(), mk.icon_rupee(),
        mk.icon_key(), mk.icon_arrow(), mk.icon_heart_container(), mk.icon_shield(),
    ]
    images = []
    for idx, (folder, name) in enumerate(ICONS):
        p = find_png(assets, folder, name) if folder else None
        if p:
            images.append(Image.open(p).convert("RGBA").resize((32, 32), Image.LANCZOS))
        else:
            if folder:
                print(f"  missing {name}, keeping generated icon")
            images.append(generated[idx])
    frames, palette = mk.rgba_frames_to_alphtest(images)
    mk.write_spr(os.path.join(out, "zl_icons.spr"), frames, mk.SPR_ALPHTEST, palette)


def rip_hookdot(assets, out):
    # gLinkAdultHookshotReticleTex: an I8 texture, drawn with PRIMITIVE red (the HUD tints it)
    p = os.path.join(assets, "objects", "object_link_boy")
    path = None
    if os.path.isdir(p):
        for f in os.listdir(p):
            if f.startswith("gLinkAdultHookshotReticleTex.") and f.endswith(".png"):
                path = os.path.join(p, f)
    if not path:
        print("  gLinkAdultHookshotReticleTex not found")
        return
    i, a = load_la(path)
    mk.write_spr(os.path.join(out, "zl_hookdot.spr"), [mk.alpha_frame(i)], mk.SPR_INDEXALPHA, mk.indexalpha_palette())


def keep_png(assets, name):
    """A gameplay_keep texture as rows of 0..1 intensity"""
    d = os.path.join(assets, "objects", "gameplay_keep")
    if os.path.isdir(d):
        for f in os.listdir(d):
            if f.startswith(name + ".") and f.endswith(".png"):
                i, _ = load_la(os.path.join(d, f))
                w, h = i.size
                px = i.load()
                return [[px[x, y] / 255.0 for x in range(w)] for y in range(h)]
    print(f"  {name} not found")
    return None


def rip_sword_effects(assets, out):
    # The spin attack (EnMThunder): its ring and the charging glow on the blade
    fire = keep_png(assets, "gSpinAttackFireTex")
    magic = keep_png(assets, "gSpinAttackMagicTex")
    if fire and magic:
        mk.bake_spin_ring(out, "zl_spinring_w.spr", fire, magic, "weak")
        mk.bake_spin_ring(out, "zl_spinring_s.spr", fire, magic, "strong")
    c1 = keep_png(assets, "gSpinAttackCharging1Tex")
    c2 = keep_png(assets, "gSpinAttackCharging2Tex")
    if c1 and c2:
        mk.bake_spin_charge(out, "zl_spincharge_b.spr", c1, c2, "weak")
        mk.bake_spin_charge(out, "zl_spincharge_o.spr", c1, c2, "strong")
    # EffectSsHitMark: 32x24 textures of which the quad shows 24x24
    marks = [keep_png(assets, f"gEffHitMark{k}Tex") for k in range(1, 9)]
    if all(marks):
        mk.bake_hitmark(out, [[row[:24] for row in m[:24]] for m in marks])


def rip_drops(assets, out):
    # EnItem00_DrawCollectible's billboards, sItemDropTex order (the frame is the texture index)
    names = ["gDropRecoveryHeartTex", "gDropBombTex", "gDropArrows1Tex", "gDropArrows2Tex",
             "gDropArrows3Tex", "gDropBombTex", "gDropDekuNutTex", "gDropDekuStickTex",
             "gDropMagicLargeTex", "gDropMagicSmallTex", "gDropDekuSeedsTex", "gDropKeySmallTex"]
    d = os.path.join(assets, "objects", "gameplay_keep")
    images = []
    for n in names:
        path = None
        if os.path.isdir(d):
            for f in os.listdir(d):
                if f.startswith(n + ".") and f.endswith(".png"):
                    path = os.path.join(d, f)
        if not path:
            print(f"  {n} not found")
            return
        images.append(Image.open(path).convert("RGBA"))
    frames, palette = mk.rgba_frames_to_alphtest(images)
    mk.write_spr(os.path.join(out, "zl_drops.spr"), frames, mk.SPR_ALPHTEST, palette)


def read_font_widths(oot):
    path = os.path.join(oot, "src", "code", "z_message.c")
    try:
        src = open(path, encoding="utf-8", errors="replace").read()
    except OSError:
        return None
    m = re.search(r"sFontWidths\[\d+\]\s*=\s*\{(.*?)\};", src, re.S)
    if not m:
        return None
    body = re.sub(r"/\*.*?\*/", "", m.group(1), flags=re.S)
    body = re.sub(r"//[^\n]*", "", body)
    vals = [float(v.rstrip("f")) for v in re.findall(r"-?\d+(?:\.\d+)?f?", body)]
    return vals


def rip_font(assets, oot, out):
    d = os.path.join(assets, "textures", "nes_font_static")
    if not os.path.isdir(d):
        print("  nes_font_static not extracted, keeping generated font")
        return
    files = {}
    for f in os.listdir(d):
        m = re.match(r"gMsgChar([0-9A-Fa-f]{2})", f)
        if m and f.endswith(".png"):
            files[int(m.group(1), 16)] = os.path.join(d, f)

    widths_oot = read_font_widths(oot)
    frames = []
    widths = []
    for c in range(32, 127):
        cell = Image.new("L", (32, 32), 0)
        w16 = widths_oot[c - 32] if widths_oot and c - 32 < len(widths_oot) else 8.0
        adv = int(round(w16 * 2))
        if c in files:
            i, a = load_la(files[c])
            glyph = mul(i, a) if Image.open(files[c]).mode in ("LA", "P", "RGBA") else i
            glyph = scale_nearest(glyph, (32, 32))
            # OoT glyphs sit at the left of their cell; the HUD centres glyphs in the cell
            shift = max(0, (32 - adv) // 2)
            cell.paste(glyph, (shift, 0))
        frames.append(mk.alpha_frame(cell))
        widths.append(adv if c != 32 else max(adv, 8))

    mk.write_spr(os.path.join(out, "zl_font.spr"), frames, mk.SPR_INDEXALPHA, mk.indexalpha_palette())
    with open(os.path.join(out, "zl_font.txt"), "w") as f:
        f.write("// Advance widths for zl_font.spr (32 px cells), ASCII 32..126, from sFontWidths\n")
        f.write(" ".join(str(w) for w in widths) + "\n")
    print(f"wrote {os.path.join(out, 'zl_font.txt')}")


# ---------------------------------------------------------------------------
# Messages: extracted/VERSION/text/message_data.h -> zl_oot_messages.txt
# ---------------------------------------------------------------------------

BOX_TYPES = {
    "TEXTBOX_TYPE_BLACK": "BLACK", "TEXTBOX_TYPE_WOODEN": "WOODEN", "TEXTBOX_TYPE_BLUE": "BLUE",
    "TEXTBOX_TYPE_OCARINA": "BLUE", "TEXTBOX_TYPE_NONE_BOTTOM": "NONE", "TEXTBOX_TYPE_NONE_NO_SHADOW": "NONE",
    "TEXTBOX_TYPE_CREDITS": "NONE",
}
BOX_POS = {
    "TEXTBOX_POS_VARIABLE": "VARIABLE", "TEXTBOX_POS_TOP": "TOP", "TEXTBOX_POS_MIDDLE": "MIDDLE",
    "TEXTBOX_POS_BOTTOM": "BOTTOM",
}
COLORS = {"DEFAULT": "WHITE", "RED": "RED", "ADJUSTABLE": "GREEN", "BLUE": "BLUE", "LIGHTBLUE": "LIGHTBLUE",
          "PURPLE": "PURPLE", "YELLOW": "YELLOW", "BLACK": "BLACK"}

# Which OoT item get text replaces which ITEM_* box (found by wording)
ITEM_ALIASES = [
    ("ITEM_BOW", ["Fairy Bow"]),
    ("ITEM_BOMB_BAG", ["Bomb Bag"]),
    ("ITEM_HOOKSHOT", ["Hookshot"]),
    ("ITEM_LONGSHOT", ["Longshot"]),
    ("ITEM_HEART_CONTAINER", ["Heart Container"]),
    ("ITEM_SMALL_KEY", ["Small Key"]),
    ("ITEM_MASTER_SWORD", ["Master Sword"]),
    ("ITEM_HYLIAN_SHIELD", ["Hylian Shield"]),
]


def tokenize_msg(body):
    """Yields ('text', str) and ('ctrl', name, arg) from a MSG( ... ) body."""
    i = 0
    n = len(body)
    while i < n:
        c = body[i]
        if c.isspace() or c == ",":
            i += 1
            continue
        if body.startswith("/*", i):
            j = body.find("*/", i)
            i = n if j < 0 else j + 2
            continue
        if c == '"':
            j = i + 1
            buf = []
            while j < n and body[j] != '"':
                if body[j] == "\\" and j + 1 < n:
                    e = body[j + 1]
                    buf.append({"n": "\n", '"': '"', "\\": "\\"}.get(e, e))
                    j += 2
                    continue
                buf.append(body[j])
                j += 1
            yield ("text", "".join(buf))
            i = j + 1
            continue
        m = re.match(r"([A-Z_0-9]+)(\(([^)]*)\))?", body[i:])
        if m:
            yield ("ctrl", m.group(1), (m.group(3) or "").strip())
            i += m.end()
            continue
        i += 1


def convert_msg(body):
    out = []
    for tok in tokenize_msg(body):
        if tok[0] == "text":
            out.append(tok[1].replace("{", "(").replace("}", ")"))
            continue
        _, name, arg = tok
        if name == "BOX_BREAK":
            out.append("\n{BOX}\n")
        elif name == "BOX_BREAK_DELAYED":
            out.append("\n{BOX:%s}\n" % arg)
        elif name == "COLOR":
            out.append("{%s}" % COLORS.get(arg, "WHITE"))
        elif name == "NAME":
            out.append("{NAME}")
        elif name == "QUICKTEXT_ENABLE":
            out.append("{INSTANT}")
        elif name == "QUICKTEXT_DISABLE":
            out.append("{/INSTANT}")
        elif name == "SHIFT":
            out.append("{SHIFT:%s}" % arg)
        elif name == "TEXT_SPEED":
            out.append("{SPEED:%s}" % arg)
        elif name == "FADE":
            out.append("{FADE:%s}" % arg)
        elif name == "FADE2":
            out.append("{FADE:%s}" % arg)
        elif name == "TWO_CHOICE":
            out.append("{CHOICE2}")
        elif name == "THREE_CHOICE":
            out.append("{CHOICE3}")
        elif name == "SFX":
            out.append("{SFX:%s}" % arg)
        elif name == "UNSKIPPABLE":
            out.append("{UNSKIPPABLE}")
        elif name in ("AWAIT_BUTTON_PRESS", "EVENT", "PERSISTENT"):
            out.append("{WAIT}")
        elif name == "POINTS":
            out.append("0")
        elif name == "TIME":
            out.append("{TIME}")
        elif name in ("MARATHON_TIME", "RACE_TIME"):
            out.append("0:00")
        elif name == "TOKENS":
            out.append("0")
        # ITEM_ICON, BACKGROUND, OCARINA, TEXTID, END, HIGHSCORE, FISH_INFO: nothing to show
    text = "".join(out)
    # Choice markers in OoT come before the option lines; ours go at the end of that box
    while True:
        m = re.search(r"\{CHOICE([23])\}\n?", text)
        if not m:
            break
        marker = "{CHOICE%s}" % m.group(1)
        rest = text[m.end():]
        cut = rest.find("\n{BOX")
        if cut < 0:
            body, tail = rest.rstrip("\n"), ""
        else:
            body, tail = rest[:cut].rstrip("\n"), rest[cut:]
        text = text[:m.start()] + body + marker.replace("{", "\x00") + tail
    text = text.replace("\x00", "{")
    lines = [l.rstrip() for l in text.split("\n")]
    while lines and not lines[-1]:
        lines.pop()
    return "\n".join(lines)


def rip_messages(extracted, out_resource):
    path = os.path.join(extracted, "text", "message_data.h")
    if not os.path.exists(path):
        print("  text/message_data.h not extracted (make setup), skipping messages")
        return
    src = open(path, encoding="utf-8", errors="replace").read()

    entries = []
    chunks = re.split(r"(?=^DEFINE_MESSAGE)", src, flags=re.M)
    for chunk in chunks:
        m = re.match(r"DEFINE_MESSAGE(_NES|_JPN|_FFFC)?\(\s*(0x[0-9A-Fa-f]+)\s*,\s*(\w+)\s*,\s*(\w+)\s*,(.*)", chunk, re.S)
        if not m or m.group(1) in ("_JPN", "_FFFC"):
            continue
        _, text_id, box_type, box_pos, rest = m.groups()
        msgs = re.findall(r"MSG\((.*?)\n\)", rest, re.S)
        if len(msgs) < 2:
            continue
        body = msgs[1]          # [jpn, nes(english), ger, fra]
        if "MISSING" in body and len(body) < 30:
            continue
        entries.append((text_id.upper().replace("0X", "0x"), BOX_TYPES.get(box_type, "BLACK"),
                        BOX_POS.get(box_pos, "VARIABLE"), convert_msg(body)))

    os.makedirs(out_resource, exist_ok=True)
    out_path = os.path.join(out_resource, "zl_oot_messages.txt")
    with open(out_path, "w", encoding="latin-1", errors="replace") as f:
        f.write("// Converted from your OoT ROM by tools/zelda/rip_from_decomp.py. Do not distribute.\n")
        f.write("// Loaded before zl_messages.txt, so the ITEM_* aliases below replace the stand-in text.\n\n")
        for name, keywords in ITEM_ALIASES:
            for tid, bt, bp, text in entries:
                flat = text.replace("\n", " ")
                if "You got" in flat and all(k in flat for k in keywords):
                    f.write(f"@{name} {bt} {bp}\n{text}\n\n")
                    break
        for tid, bt, bp, text in entries:
            f.write(f"@{tid} {bt} {bp}\n{text}\n\n")
    print(f"wrote {out_path} ({len(entries)} messages)")


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--oot", required=True, help="path to your zeldaret/oot checkout (after make setup)")
    ap.add_argument("--version", default="ntsc-1.0")
    ap.add_argument("--out", default=os.path.join(here, "..", "..", "zelda"))
    args = ap.parse_args()

    extracted = os.path.join(args.oot, "extracted", args.version)
    assets = os.path.join(extracted, "assets")
    if not os.path.isdir(assets):
        sys.exit(f"{assets} not found. Run 'make setup VERSION={args.version}' in the decomp first.")

    # Ripped art goes in sprites/oot/, which the HUD prefers and git ignores
    sprites = os.path.join(args.out, "sprites", "oot")
    print("hearts");     rip_hearts(assets, sprites)
    print("hud icons");  rip_hud_icons(assets, sprites)
    print("magic meter"); rip_magic(assets, sprites)
    print("buttons");    rip_buttons(assets, sprites)
    print("item icons"); rip_item_icons(assets, sprites)
    print("hookshot dot"); rip_hookdot(assets, sprites)
    print("sword effects"); rip_sword_effects(assets, sprites)
    print("drops");      rip_drops(assets, sprites)
    print("font");       rip_font(assets, args.oot, sprites)
    print("messages");   rip_messages(extracted, os.path.join(args.out, "resource"))


if __name__ == "__main__":
    main()
