#!/usr/bin/env python3
"""
Zelda 64 port: build models/player/link/link.mdl from YOUR copy of Ocarina of Time.

Reads the zeldaret/oot decomp's extracted assets (run `make setup` there first) and writes a
Half-Life studio model source: SMD meshes, SMD animations, 8 bit BMP textures and a QC. If
studiomdl is found (or given with --studiomdl), it also compiles link.mdl.

    python3 make_link_model.py --oot /path/to/oot [--version ntsc-1.0] [--studiomdl path]

What it converts (every name below is from the decomp):
  skeleton     gLinkAdultSkel (object_link_boy), 21 limbs -> 21 bones
  mesh         each limb's near display list (dLists[0]); F3DEX2 commands are interpreted:
               gsSPVertex loads vertices under the current matrix, gsSPMatrix(0x0D......)
               switches to another limb's matrix (OoT stitches joints by loading a few
               vertices of every limb under its parent's matrix), gsSP1Triangle/2Triangles
               emit triangles with the current texture and colour combiner
  materials    gsDPSetCombineLERP is evaluated: TEXEL0 x PRIMITIVE / ENVIRONMENT colours are
               baked into the texture (ENVIRONMENT is the tunic colour Player sets, Kokiri
               green by default). Mirrored texture axes are unrolled into a texture twice
               the size. Faces drawn with culling off get a back face.
  face         segment 0x08 / 0x09 (eyes / mouth) become gLinkAdultEyesOpenTex /
               gLinkAdultMouthClosedTex, plus a skin family for blinking (open/half/closed)
  equipment    bodygroups swap the hand and sheath display lists like Player_OverrideLimbDraw:
               lhand: open, fist, master sword
               rhand: open, fist, hylian shield, bow, hookshot
               back:  sword + shield, shield + empty sheath, empty sheath, sword + sheath
  animations   gPlayerAnim_* (gameplay_keep header + link_animetion frame data):
               per frame Vec3s root translation, 21 Vec3s limb rotations (binary angles,
               applied Z then Y then X like Matrix_TranslateRotateZYX) and a face index.
               Sequence names are the OoT names without "gPlayerAnim_" (what zl_anim.cpp
               looks for), at OoT's 20 frames per second.

Units: Player is drawn at actor scale 0.01; 1 OoT unit = 72/68 HL units (zl_shared.h), so
model units x 0.01 x 72/68. Y up / facing +Z becomes Z up / facing +X, and the feet go 36
units under the origin like player.mdl.

Nothing this writes may be redistributed: it comes from your ROM. The output folders are git
ignored. Needs Pillow.
"""

import argparse
import math
import os
import re
import shutil
import subprocess
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))

SCALE = 0.01 * 72.0 / 68.0
FEET_Z = -36.0
BINANG = 2.0 * math.pi / 65536.0

TUNICS = {
    "kokiri": (30, 105, 27),
    "goron": (100, 20, 0),
    "zora": (0, 60, 100),
}

# Equipment display lists, drawn on the hand / sheath limbs (Player_OverrideLimbDrawGameplayCommon)
BODYGROUPS = [
    ("lhand", 15, [
        ("open", "gLinkAdultLeftHandNearDL"),
        ("fist", "gLinkAdultLeftHandClosedNearDL"),
        ("sword", "gLinkAdultLeftHandHoldingMasterSwordNearDL"),
    ]),
    ("rhand", 18, [
        ("open", "gLinkAdultRightHandNearDL"),
        ("fist", "gLinkAdultRightHandClosedNearDL"),
        ("shield", "gLinkAdultRightHandHoldingHylianShieldNearDL"),
        ("bow", "gLinkAdultRightHandHoldingBowNearDL"),
        ("hookshot", "gLinkAdultRightHandHoldingHookshotNearDL"),
    ]),
    ("back", 19, [
        ("sword_shield", "gLinkAdultHylianShieldSwordAndSheathNearDL"),
        ("shield", "gLinkAdultHylianShieldAndSheathNearDL"),
        ("sheath", "gLinkAdultSheathNearDL"),
        ("sword", "gLinkAdultMasterSwordAndSheathNearDL"),
    ]),
]

BONE_NAMES = [
    "Root", "Waist", "LowerControl", "R_Thigh", "R_Shin", "R_Foot", "L_Thigh", "L_Shin", "L_Foot",
    "Bip01 Spine", "Head", "Hat", "Collar", "L_Shoulder", "L_Forearm", "L_Hand",
    "R_Shoulder", "R_Forearm", "R_Hand", "Sheath", "Torso",
]

EYE_SKINS = ["gLinkAdultEyesOpenTex", "gLinkAdultEyesHalfTex", "gLinkAdultEyesClosedTex"]
MOUTH_TEX = "gLinkAdultMouthClosedTex"


# ---------------------------------------------------------------------------------------------
# Small maths
# ---------------------------------------------------------------------------------------------
def mat_mul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]


def mat_vec(m, v):
    return [m[0][0] * v[0] + m[0][1] * v[1] + m[0][2] * v[2],
            m[1][0] * v[0] + m[1][1] * v[1] + m[1][2] * v[2],
            m[2][0] * v[0] + m[2][1] * v[1] + m[2][2] * v[2]]


def euler_to_mat(x, y, z):
    """R = Rz * Ry * Rx, studiomdl's AngleMatrix and OoT's Matrix_RotateZYX order."""
    cx, sx = math.cos(x), math.sin(x)
    cy, sy = math.cos(y), math.sin(y)
    cz, sz = math.cos(z), math.sin(z)
    rx = [[1, 0, 0], [0, cx, -sx], [0, sx, cx]]
    ry = [[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]]
    rz = [[cz, -sz, 0], [sz, cz, 0], [0, 0, 1]]
    return mat_mul(rz, mat_mul(ry, rx))


def mat_to_euler(m):
    sy = -m[2][0]
    sy = max(-1.0, min(1.0, sy))
    y = math.asin(sy)
    if abs(sy) < 0.99999:
        x = math.atan2(m[2][1], m[2][2])
        z = math.atan2(m[1][0], m[0][0])
    else:
        x = math.atan2(-m[1][2], m[1][1])
        z = 0.0
    return x, y, z


# OoT (x, y, z) -> HL (z, x, y): Y up / +Z forward becomes Z up / +X forward
AXIS = [[0, 0, 1], [1, 0, 0], [0, 1, 0]]


def to_hl(p):
    q = mat_vec(AXIS, p)
    return [q[0] * SCALE, q[1] * SCALE, q[2] * SCALE + FEET_Z]


# ---------------------------------------------------------------------------------------------
# Reading the decomp's C sources
# ---------------------------------------------------------------------------------------------
class Assets:
    def __init__(self, extracted):
        self.extracted = extracted
        self.obj = os.path.join(extracted, "assets", "objects", "object_link_boy")
        self.keep = os.path.join(extracted, "assets", "objects", "gameplay_keep")
        self.anim_data = os.path.join(extracted, "assets", "misc", "link_animetion")
        self.search = [self.obj, self.keep]
        self._vtx = {}
        self._png = {}
        self._listing = {d: os.listdir(d) for d in self.search if os.path.isdir(d)}

    def source(self, name):
        for d in self.search:
            p = os.path.join(d, name + ".inc.c")
            if os.path.exists(p):
                return open(p).read()
        return None

    def vtx(self, name):
        if name not in self._vtx:
            src = self.source(name)
            if src is None:
                raise KeyError(f"vertex array {name} not found")
            out = []
            for m in re.finditer(r"VTX\(([^)]*)\)", src):
                vals = [int(v.strip(), 0) for v in m.group(1).split(",")]
                out.append(vals)
            self._vtx[name] = out
        return self._vtx[name]

    def png(self, name):
        if name not in self._png:
            self._png[name] = None
            for d, files in self._listing.items():
                for f in files:
                    if f.startswith(name + ".") and f.endswith(".png"):
                        self._png[name] = os.path.join(d, f)
                        break
                if self._png[name]:
                    break
        return self._png[name]


def split_macros(src):
    """Yields (name, [args]) for each gsXxx(...) in a display list source."""
    i = 0
    n = len(src)
    while True:
        m = re.compile(r"\b(gs\w+)\s*\(").search(src, i)
        if not m:
            return
        name = m.group(1)
        j = m.end()
        depth = 1
        args = []
        cur = ""
        while j < n and depth:
            c = src[j]
            if c == "(":
                depth += 1
                cur += c
            elif c == ")":
                depth -= 1
                if depth:
                    cur += c
            elif c == "," and depth == 1:
                args.append(cur.strip())
                cur = ""
            else:
                cur += c
            j += 1
        if cur.strip():
            args.append(cur.strip())
        yield name, args
        i = j


def c_int(s):
    s = s.strip()
    try:
        return int(s, 0)
    except ValueError:
        return None


def parse_limbs(assets):
    listing = assets.source("gLinkAdultLimbs")
    names = re.findall(r"&(\w+)", listing)
    limbs = []
    for name in names:
        src = assets.source(name)
        nums = re.findall(r"(-?\d+),\s*//\s*[xyz]\b", src)
        joint = [int(v) for v in nums[:3]]
        child = re.search(r"/\*\s*(\d+)\s*\*/[^\n]*//\s*child", src)
        sib = re.search(r"/\*\s*(\d+)\s*\*/[^\n]*//\s*sibling", src)
        dl_names = re.findall(r"^\s*(\w+),\s*//\s*[01]\s*$", src, re.M)
        child = int(child.group(1)) if child else 255
        sib = int(sib.group(1)) if sib else 255
        near = dl_names[0] if dl_names and dl_names[0] != "NULL" else None
        limbs.append({"name": name, "joint": joint, "child": child, "sibling": sib, "dl": near})
    # Parents from child / sibling links
    parent = [-1] * len(limbs)

    def walk(idx, par):
        while idx != 255:
            parent[idx] = par
            if limbs[idx]["child"] != 255:
                walk(limbs[idx]["child"], idx)
            idx = limbs[idx]["sibling"]

    walk(0, -1)
    for i, l in enumerate(limbs):
        l["parent"] = parent[i]
    return limbs


# ---------------------------------------------------------------------------------------------
# F3DEX2 display list interpreter
# ---------------------------------------------------------------------------------------------
G_CULL_BACK = 0x400
G_CULL_FRONT = 0x200
G_LIGHTING = 0x20000
G_TEXTURE_GEN = 0x40000

GEO_FLAGS = {"G_CULL_BACK": G_CULL_BACK, "G_CULL_FRONT": G_CULL_FRONT,
             "G_CULL_BOTH": G_CULL_BACK | G_CULL_FRONT, "G_LIGHTING": G_LIGHTING,
             "G_TEXTURE_GEN": G_TEXTURE_GEN}

CC_PRESETS = {
    # (a, b, c, d) for colour of cycle 1 and cycle 2 that we care about
    "G_CC_MODULATEIDECALA_PRIM": ("TEXEL0", "0", "PRIMITIVE", "0"),
    "G_CC_MODULATEIDECALA": ("TEXEL0", "0", "SHADE", "0"),
    "G_CC_MODULATEIA_PRIM": ("TEXEL0", "0", "PRIMITIVE", "0"),
    "G_CC_PASS2": ("0", "0", "0", "COMBINED"),
}


def combine_inputs(cyc1, cyc2):
    """Returns the set of inputs the final colour is a product of, e.g. {TEXEL0, SHADE, ENVIRONMENT}."""
    def term(cyc, prev):
        a, b, c, d = cyc
        if a == "0" and b == "0" and c == "0":
            return prev if d == "COMBINED" else ({d} if d not in ("0",) else set())
        res = set()
        for x in (a, c):
            if x == "COMBINED":
                res |= prev
            elif x not in ("0", "1"):
                res.add(x)
        return res
    first = term(cyc1, set())
    return term(cyc2, first)


def geo_bits(arg):
    bits = 0
    for tok in re.split(r"\s*\|\s*", arg):
        bits |= GEO_FLAGS.get(tok.strip(), 0)
    return bits


class Material:
    __slots__ = ("tex", "w", "h", "mirror_s", "mirror_t", "clamp_s", "clamp_t", "tint", "textured", "alpha_tex", "solid",
                 "chrome")

    def key(self):
        return (self.tex, self.mirror_s, self.mirror_t, self.tint, self.textured, self.alpha_tex, self.solid, self.chrome,
                self.clamp_s, self.clamp_t)


class DLRunner:
    def __init__(self, assets, flex_limbs, env_color):
        self.assets = assets
        self.flex = flex_limbs
        self.env = env_color
        self.tris = []   # (material, [(limb, pos(oot local), normal, s, t), x3])

    def run(self, dl_name, limb):
        self.mtx = limb
        self.vbuf = [None] * 64
        self.tex = None
        self.tex_w = self.tex_h = 32
        self.cms = self.cmt = ""
        self.tscale = (1.0, 1.0)
        self.prim = (255, 255, 255)
        self.envc = self.env
        self.cc = (("TEXEL0", "0", "SHADE", "0"), ("0", "0", "0", "COMBINED"))
        self.geo = G_CULL_BACK | G_LIGHTING
        self._run(dl_name, 0)

    def _run(self, dl_name, depth):
        src = self.assets.source(dl_name)
        if src is None:
            print(f"  warning: display list {dl_name} not found")
            return
        for name, args in split_macros(src):
            if name == "gsSPVertex":
                m = re.match(r"&?(\w+)\[(\d+)\]", args[0])
                arr, start = m.group(1), int(m.group(2))
                count, v0 = c_int(args[1]), c_int(args[2])
                verts = self.assets.vtx(arr)
                for i in range(count):
                    x, y, z, s, t, r, g, b, a = verts[start + i]
                    nrm = [((r + 128) & 0xFF) - 128, ((g + 128) & 0xFF) - 128, ((b + 128) & 0xFF) - 128]
                    self.vbuf[v0 + i] = (self.mtx, (x, y, z), nrm, s * self.tscale[0], t * self.tscale[1], (r, g, b, a),
                                         bool(self.geo & G_LIGHTING), bool(self.geo & G_TEXTURE_GEN))
            elif name == "gsSPMatrix":
                addr = c_int(args[0])
                if addr is not None and (addr >> 24) == 0x0D:
                    idx = (addr & 0xFFFFFF) // 0x40
                    if idx < len(self.flex):
                        self.mtx = self.flex[idx]
            elif name == "gsSP1Triangle":
                self._tri([c_int(a) for a in args[:3]])
            elif name == "gsSP2Triangles":
                v = [c_int(a) for a in args]
                self._tri(v[0:3])
                self._tri(v[4:7])
            elif name == "gsSPTexture":
                s, t = c_int(args[0]), c_int(args[1])
                self.tscale = ((s + 1) / 65536.0 if s == 0xFFFF else s / 65536.0,
                               (t + 1) / 65536.0 if t == 0xFFFF else t / 65536.0)
            elif name in ("gsDPLoadTextureBlock", "gsDPLoadTextureBlock_4b", "gsDPLoadMultiBlock", "gsDPLoadMultiBlock_4b"):
                off = 1 if name.startswith("gsDPLoadMultiBlock") else 0
                tex = args[0]
                addr = c_int(tex)
                if addr is not None:
                    seg = addr >> 24
                    tex = {8: EYE_SKINS[0], 9: MOUTH_TEX}.get(seg)
                self.tex = tex
                if name.endswith("_4b"):
                    w, h = args[2 + off], args[3 + off]
                    cms, cmt = args[5 + off], args[6 + off]
                else:
                    w, h = args[3 + off], args[4 + off]
                    cms, cmt = args[6 + off], args[7 + off]
                self.tex_w = c_int(w) if c_int(w) is not None else None
                self.tex_h = c_int(h) if c_int(h) is not None else None
                self.cms, self.cmt = cms, cmt
            elif name == "gsDPSetPrimColor":
                self.prim = (c_int(args[2]), c_int(args[3]), c_int(args[4]))
            elif name == "gsDPSetEnvColor":
                self.envc = (c_int(args[0]), c_int(args[1]), c_int(args[2]))
            elif name == "gsDPSetCombineLERP":
                self.cc = (tuple(args[0:4]), tuple(args[8:12]))
                self.cc_alpha = (tuple(args[4:8]), tuple(args[12:16]))
            elif name == "gsDPSetCombineMode":
                c1 = CC_PRESETS.get(args[0], ("TEXEL0", "0", "SHADE", "0"))
                c2 = CC_PRESETS.get(args[1], ("0", "0", "0", "COMBINED"))
                self.cc = (c1, c2)
            elif name == "gsSPSetGeometryMode":
                self.geo |= geo_bits(args[0])
            elif name == "gsSPClearGeometryMode":
                self.geo &= ~geo_bits(args[0])
            elif name == "gsSPLoadGeometryMode":
                self.geo = geo_bits(args[0])
            elif name == "gsSPDisplayList" or name == "gsSPBranchList":
                target = args[0]
                if c_int(target) is None and depth < 8:
                    self._run(target, depth + 1)
                if name == "gsSPBranchList":
                    return
            elif name == "gsSPEndDisplayList":
                return

    def _material(self):
        m = Material()
        inputs = combine_inputs(*self.cc)
        m.textured = "TEXEL0" in inputs and self.tex is not None
        tint = [255, 255, 255]
        for src, col in (("PRIMITIVE", self.prim), ("ENVIRONMENT", self.envc)):
            if src in inputs:
                tint = [tint[i] * col[i] // 255 for i in range(3)]
        m.tint = tuple(tint)
        m.solid = not m.textured
        m.tex = self.tex if m.textured else None
        m.w, m.h = self.tex_w, self.tex_h
        m.mirror_s = "G_TX_MIRROR" in self.cms
        m.mirror_t = "G_TX_MIRROR" in self.cmt
        m.clamp_s = "G_TX_CLAMP" in self.cms
        m.clamp_t = "G_TX_CLAMP" in self.cmt
        alpha = getattr(self, "cc_alpha", None)
        m.alpha_tex = bool(alpha) and any("TEXEL0" in cyc for cyc in alpha)
        if not m.textured:
            m.mirror_s = m.mirror_t = m.clamp_s = m.clamp_t = False
            m.alpha_tex = False
        return m

    def _tri(self, idx):
        vs = [self.vbuf[i] for i in idx]
        if any(v is None for v in vs):
            return
        mat = self._material()
        # G_TEXTURE_GEN: the RSP makes the UVs from the normals (OoT's shiny metal); that is
        # exactly Half-Life's chrome
        mat.chrome = mat.textured and any(v[7] for v in vs)
        if mat.chrome:
            mat.mirror_s = mat.mirror_t = mat.clamp_s = mat.clamp_t = False
        culled = bool(self.geo & G_CULL_BACK)
        self.tris.append((mat, vs, not culled))


# ---------------------------------------------------------------------------------------------
# Textures
# ---------------------------------------------------------------------------------------------
def n64_address(x, size, mirror, clamp):
    """Texel index the RDP reads for texel coordinate x on one axis (G_TX_MIRROR / WRAP / CLAMP)."""
    period = size * 2 if mirror else size
    if clamp:
        x = min(max(x, 0), period - 1)
    x %= period
    if mirror and x >= size:
        x = period - 1 - x
    return x


class TextureBank:
    """Every material gets a texture covering exactly the texel area its UVs span, sampled with
    the N64's addressing, so the SMD's UVs stay in 0..1. studiomdl clamps UVs to -1..2 and
    repacks them, which breaks mirrored and clamped textures (the Hylian Shield's crest, the
    Gossip Stone's eye) otherwise."""

    MAX_SIZE = 256

    def __init__(self, assets, outdir):
        self.assets = assets
        self.outdir = outdir
        self.made = {}     # (material key, texel range) -> (bmp name, width, height, masked)
        self.masked = set()
        self.names = set()

    def source_image(self, mat, tex):
        path = self.assets.png(tex)
        if not path:
            print(f"  warning: texture {tex} not extracted, using grey")
            img = Image.new("RGBA", (mat.w or 32, mat.h or 32), (160, 160, 160, 255))
        else:
            img = Image.open(path).convert("RGBA")
        if mat.tint != (255, 255, 255):
            r, g, b, a = img.split()
            r = r.point(lambda v, k=mat.tint[0]: v * k // 255)
            g = g.point(lambda v, k=mat.tint[1]: v * k // 255)
            b = b.point(lambda v, k=mat.tint[2]: v * k // 255)
            img = Image.merge("RGBA", (r, g, b, a))
        return img

    def name_for(self, mat, rng=None, tex_override=None):
        """rng: (x0, x1, y0, y1) in whole texels, the area the material's UVs cover."""
        tex = tex_override or mat.tex
        mkey = mat.key() if tex_override is None else (tex,) + mat.key()[1:]
        if mat.solid or mat.chrome or rng is None:
            rng = None
        key = mkey + (rng,)
        if key in self.made:
            return self.made[key]
        tint_hex = "%02x%02x%02x" % mat.tint
        if mat.solid:
            base = "solid_" + tint_hex
            img = Image.new("RGBA", (8, 8), mat.tint + (255,))
        else:
            src = self.source_image(mat, tex)
            short = re.sub(r"^g(Link(Adult)?)?", "", tex).replace("Tex", "")
            if mat.chrome:
                # studiomdl flags any texture with "chrome" in its name; the engine wants 64x64
                img = src.resize((64, 64), Image.BILINEAR)
                base = f"{short}_{tint_hex}_chrome"
            else:
                x0, x1, y0, y1 = rng
                w, h = x1 - x0, y1 - y0
                sw, sh = src.size
                step = max(1.0, w / self.MAX_SIZE, h / self.MAX_SIZE)
                ow, oh = max(1, int(w / step)), max(1, int(h / step))
                img = Image.new("RGBA", (ow, oh))
                sp, dp = src.load(), img.load()
                for yy in range(oh):
                    ty = n64_address(int(y0 + yy * step), sh, mat.mirror_t, mat.clamp_t)
                    for xx in range(ow):
                        tx = n64_address(int(x0 + xx * step), sw, mat.mirror_s, mat.clamp_s)
                        dp[xx, yy] = sp[tx, ty]
                base = f"{short}_{tint_hex}"
        name = base[:40]
        n = 2
        while name + ".bmp" in self.names:
            name = f"{base[:40]}_{n}"
            n += 1
        name += ".bmp"
        self.names.add(name)
        masked = mat.alpha_tex and img.getextrema()[3][0] < 128
        save_bmp8(img, os.path.join(self.outdir, name), masked)
        if masked:
            self.masked.add(name)
        info = (name, img.size[0], img.size[1], masked)
        self.made[key] = info
        return info


def save_bmp8(img, path, masked):
    """8 bit BMP with a full 256 entry palette, the only kind studiomdl's bmpread.c takes.
    Masked textures keep index 255 for the see-through pixels."""
    rgb = img.convert("RGB")
    ncol = 255 if masked else 256
    q = rgb.quantize(colors=ncol, method=Image.MEDIANCUT)
    pal = (q.getpalette() or [])[:ncol * 3]
    pal += [0] * (768 - len(pal))
    if masked:
        pal[255 * 3:256 * 3] = [0, 0, 255]
    w, h = img.size
    idx = bytearray(q.tobytes())
    if masked:
        alpha = img.split()[3].tobytes()
        for i in range(w * h):
            if alpha[i] < 128:
                idx[i] = 255
    stride = (w + 3) & ~3
    pixels = bytearray()
    for y in range(h - 1, -1, -1):
        row = idx[y * w:(y + 1) * w]
        pixels += row + bytes(stride - w)
    palette = bytearray()
    for i in range(256):
        r, g, b = pal[i * 3:i * 3 + 3]
        palette += bytes((b, g, r, 0))
    import struct
    off = 14 + 40 + 1024
    head = struct.pack("<2sIHHI", b"BM", off + len(pixels), 0, 0, off)
    info = struct.pack("<IiiHHIIiiII", 40, w, h, 1, 8, 0, len(pixels), 2835, 2835, 256, 256)
    with open(path, "wb") as f:
        f.write(head + info + palette + pixels)


# ---------------------------------------------------------------------------------------------
# SMD writing
# ---------------------------------------------------------------------------------------------
def ref_world(limbs):
    """Model space position of every limb with no rotation applied (the reference pose)."""
    pos = []
    for i, l in enumerate(limbs):
        p = l["parent"]
        base = pos[p] if p >= 0 else [0, 0, 0]
        pos.append([base[k] + l["joint"][k] for k in range(3)])
    return pos


def bone_frame(limbs, rots, root_pos):
    """Lines of one SMD skeleton frame. rots[i] is limb i's (x, y, z) in radians."""
    lines = []
    for i, l in enumerate(limbs):
        rx, ry, rz = rots[i]
        if i == 0:
            m = mat_mul(AXIS, euler_to_mat(rx, ry, rz))
            ex, ey, ez = mat_to_euler(m)
            t = to_hl(root_pos)
        else:
            ex, ey, ez = rx, ry, rz
            t = [l["joint"][k] * SCALE for k in range(3)]
        lines.append(f"{i} {t[0]:.6f} {t[1]:.6f} {t[2]:.6f} {ex:.6f} {ey:.6f} {ez:.6f}")
    return lines


def smd_header(limbs):
    out = ["version 1", "nodes"]
    for i, l in enumerate(limbs):
        out.append(f'{i} "{BONE_NAMES[i] if i < len(BONE_NAMES) else l["name"]}" {l["parent"]}')
    out.append("end")
    return out


def write_mesh_smd(path, limbs, tris, bank):
    world = ref_world(limbs)
    rest = [(0.0, 0.0, 0.0)] * len(limbs)
    out = smd_header(limbs)
    out += ["skeleton", "time 0"] + bone_frame(limbs, rest, limbs[0]["joint"]) + ["end", "triangles"]

    # The texel area each material's UVs span (s / t are 10.5 fixed point texels)
    ranges = {}
    for mat, vs, two_sided in tris:
        if mat.solid or mat.chrome:
            continue
        k = mat.key()
        xs = [v[3] / 32.0 for v in vs]
        ys = [v[4] / 32.0 for v in vs]
        r = ranges.get(k)
        lo_x, hi_x, lo_y, hi_y = min(xs), max(xs), min(ys), max(ys)
        if r:
            lo_x, hi_x, lo_y, hi_y = min(lo_x, r[0]), max(hi_x, r[1]), min(lo_y, r[2]), max(hi_y, r[3])
        ranges[k] = (lo_x, hi_x, lo_y, hi_y)
    texel_range = {}
    for k, (lo_x, hi_x, lo_y, hi_y) in ranges.items():
        x0, x1 = int(math.floor(lo_x + 1e-4)), int(math.ceil(hi_x - 1e-4))
        y0, y1 = int(math.floor(lo_y + 1e-4)), int(math.ceil(hi_y - 1e-4))
        texel_range[k] = (x0, max(x1, x0 + 1), y0, max(y1, y0 + 1))

    count = 0
    for mat, vs, two_sided in tris:
        rng = texel_range.get(mat.key())
        name, tw, th, masked = bank.name_for(mat, rng)
        rows = []
        for limb, p, n, s, t, col, lit, tgen in vs:
            wp = [world[limb][k] + p[k] for k in range(3)]
            hp = to_hl(wp)
            nn = mat_vec(AXIS, n)
            ln = math.sqrt(sum(c * c for c in nn)) or 1.0
            nn = [c / ln for c in nn]
            if rng is None:
                u, v = 0.5, 0.5
            else:
                x0, x1, y0, y1 = rng
                u = ( s / 32.0 - x0 ) / ( x1 - x0 )
                v = ( t / 32.0 - y0 ) / ( y1 - y0 )
            rows.append((limb, hp, nn, u, 1.0 - v))
        faces = [rows]
        if two_sided:
            faces.append([(r[0], r[1], [-c for c in r[2]], r[3], r[4]) for r in reversed(rows)])
        for f in faces:
            out.append(name)
            for limb, hp, nn, u, v in f:
                out.append(f"{limb} {hp[0]:.6f} {hp[1]:.6f} {hp[2]:.6f} {nn[0]:.6f} {nn[1]:.6f} {nn[2]:.6f} {u:.6f} {v:.6f}")
            count += 1
    out.append("end")
    with open(path, "w", newline="\n") as f:
        f.write("\n".join(out) + "\n")
    return count


def read_anim(assets, name):
    """Frames of gPlayerAnim_<name>: list of (root_pos, [rot x 21], face)."""
    path = os.path.join(assets.anim_data, f"gPlayerAnim_{name}_Data.inc.c")
    if not os.path.exists(path):
        return None
    vals = [int(v, 0) for v in re.findall(r"(-?0x[0-9A-Fa-f]+|-?\d+),\s*//", open(path).read())]
    per = 22 * 3 + 1
    frames = []
    for f in range(len(vals) // per):
        chunk = vals[f * per:(f + 1) * per]
        root = chunk[0:3]
        rots = [tuple(chunk[3 + i * 3 + k] * BINANG for k in range(3)) for i in range(21)]
        frames.append((root, rots, chunk[-1]))
    return frames


def write_anim_smd(path, limbs, frames, keep_root_xz):
    out = smd_header(limbs) + ["skeleton"]
    rest_xz = limbs[0]["joint"]
    for t, (root, rots, face) in enumerate(frames):
        rp = list(root)
        if not keep_root_xz:
            rp[0], rp[2] = rest_xz[0], rest_xz[2]
        out.append(f"time {t}")
        out += bone_frame(limbs, rots, rp)
    out.append("end")
    with open(path, "w", newline="\n") as f:
        f.write("\n".join(out) + "\n")


def wanted_animations():
    """(sequence name, loop) from dlls/zl_anim.cpp, so the model has what the game asks for."""
    src = open(os.path.join(REPO, "dlls", "zl_anim.cpp")).read()
    seen = []
    for m in re.finditer(r'\{\s*"gPlayerAnim_(\w+)",\s*\{[^}]*\},\s*[^,]+,\s*(TRUE|FALSE)\s*\}', src):
        name, loop = m.group(1), m.group(2) == "TRUE"
        if name not in [s for s, _ in seen]:
            seen.append((name, loop))
    return seen


# ---------------------------------------------------------------------------------------------
# Preview: software render of the model posed by animation frames, to check the conversion
# without the game. Skins each vertex exactly like the engine: pose * inverse(reference).
# ---------------------------------------------------------------------------------------------
def read_smd(path):
    lines = open(path).read().split("\n")
    parents = {}
    i = lines.index("nodes") + 1
    while lines[i] != "end":
        idx, rest = lines[i].split(" ", 1)
        parents[int(idx)] = int(rest.rsplit(" ", 1)[1])
        i += 1
    frames = []
    i = lines.index("skeleton") + 1
    cur = None
    while lines[i] != "end":
        if lines[i].startswith("time"):
            cur = {}
            frames.append(cur)
        else:
            p = lines[i].split()
            cur[int(p[0])] = [float(v) for v in p[1:7]]
        i += 1
    tris = []
    if "triangles" in lines:
        i = lines.index("triangles") + 1
        while lines[i] != "end":
            tex = lines[i]
            vs = []
            for k in range(3):
                p = lines[i + 1 + k].split()
                vs.append((int(p[0]), [float(v) for v in p[1:4]], float(p[7]), float(p[8])))
            tris.append((tex, vs))
            i += 4
    return parents, frames, tris


def world_mats(parents, frame):
    out = {}
    for b in sorted(parents):
        t = frame[b]
        r = euler_to_mat(t[3], t[4], t[5])
        if parents[b] < 0:
            out[b] = (r, t[0:3])
        else:
            pr, pt = out[parents[b]]
            out[b] = (mat_mul(pr, r), [a + b2 for a, b2 in zip(mat_vec(pr, t[0:3]), pt)])
    return out


def transpose(m):
    return [[m[j][i] for j in range(3)] for i in range(3)]


def render(tris, texdir, size, proj, texcache):
    W = H = size
    img = Image.new("RGB", (W, H), (40, 40, 60))
    zbuf = [[-1e9] * W for _ in range(H)]
    px = img.load()
    sc = H / 84.0
    for tex, vs in tris:
        if tex not in texcache:
            try:
                texcache[tex] = Image.open(os.path.join(texdir, tex)).convert("RGB")
            except OSError:
                texcache[tex] = Image.new("RGB", (8, 8), (255, 0, 255))
        ti = texcache[tex]
        tw, th = ti.size
        tp = ti.load()
        pts = []
        for p, u, v in vs:
            a, b, d = proj(*p)
            pts.append((W / 2 + a * sc, H / 2 - (b - 0) * sc, d, u, v))
        (x0, y0, d0, u0, v0), (x1, y1, d1, u1, v1), (x2, y2, d2, u2, v2) = pts
        area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)
        if area <= 0:   # facing away (SMD triangles wind counter-clockwise seen from the front)
            continue
        minx, maxx = max(0, int(min(x0, x1, x2))), min(W - 1, int(max(x0, x1, x2)) + 1)
        miny, maxy = max(0, int(min(y0, y1, y2))), min(H - 1, int(max(y0, y1, y2)) + 1)
        for yy in range(miny, maxy + 1):
            for xx in range(minx, maxx + 1):
                w0 = ((x1 - xx) * (y2 - yy) - (x2 - xx) * (y1 - yy)) / area
                w1 = ((x2 - xx) * (y0 - yy) - (x0 - xx) * (y2 - yy)) / area
                w2 = 1 - w0 - w1
                if w0 < 0 or w1 < 0 or w2 < 0:
                    continue
                d = w0 * d0 + w1 * d1 + w2 * d2
                if d <= zbuf[yy][xx]:
                    continue
                zbuf[yy][xx] = d
                u = (w0 * u0 + w1 * u1 + w2 * u2) % 1.0
                v = (w0 * v0 + w1 * v1 + w2 * v2) % 1.0
                px[xx, yy] = tp[int(u * tw) % tw, int((1 - v) * th) % th]
    return img


def preview(srcdir, png_path, shots):
    """shots: list of (smd parts, animation name or None, frame)."""
    texcache = {}
    size = 256
    views = [lambda x, y, z: (-y, z, x), lambda x, y, z: (x, z, y)]   # front (looking down -X), side
    sheet = Image.new("RGB", (size * len(views), size * len(shots)), (40, 40, 60))
    for row, (parts, anim, frame) in enumerate(shots):
        tris = []
        parents = None
        for part in parts:
            parents, ref, t = read_smd(os.path.join(srcdir, part + ".smd"))
            tris += t
        refw = world_mats(parents, ref[0])
        if anim:
            _, aframes, _ = read_smd(os.path.join(srcdir, "anims", anim + ".smd"))
            posew = world_mats(parents, aframes[min(frame, len(aframes) - 1)])
        else:
            posew = refw
        posed = []
        for tex, vs in tris:
            out = []
            for bone, p, u, v in vs:
                rr, rt = refw[bone]
                pr, pt = posew[bone]
                local = mat_vec(transpose(rr), [p[k] - rt[k] for k in range(3)])
                wp = [a + b for a, b in zip(mat_vec(pr, local), pt)]
                out.append((wp, u, v))
            posed.append((tex, out))
        for col, proj in enumerate(views):
            sheet.paste(render(posed, srcdir, size, proj, texcache), (col * size, row * size))
    sheet.save(png_path)


# ---------------------------------------------------------------------------------------------
def find_studiomdl(given):
    if given:
        return given
    for name in ("studiomdl", "studiomdl.exe"):
        p = shutil.which(name)
        if p:
            return p
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--oot", required=True, help="zeldaret/oot checkout after make setup")
    ap.add_argument("--version", default="ntsc-1.0")
    ap.add_argument("--src", default=os.path.join(REPO, "tools", "zelda", "build", "link"),
                    help="where the SMD / BMP / QC go (git ignored)")
    ap.add_argument("--out", default=os.path.join(REPO, "zelda", "models", "player", "link"),
                    help="where link.mdl goes (git ignored)")
    ap.add_argument("--tunic", choices=sorted(TUNICS), default="kokiri")
    ap.add_argument("--studiomdl", help="path to studiomdl (else looked up on PATH)")
    ap.add_argument("--all-anims", action="store_true", help="convert every gPlayerAnim, not just the ones the game uses")
    ap.add_argument("--keep-root-motion", action="store_true", help="keep the root's X/Z movement from the animations")
    ap.add_argument("--preview", action="store_true", help="render preview.png of the reference mesh")
    args = ap.parse_args()

    extracted = os.path.join(args.oot, "extracted", args.version)
    assets = Assets(extracted)
    if not os.path.isdir(assets.obj):
        sys.exit(f"{assets.obj} not found. Run 'make setup VERSION={args.version}' in the decomp first.")

    os.makedirs(args.src, exist_ok=True)
    os.makedirs(os.path.join(args.src, "anims"), exist_ok=True)

    limbs = parse_limbs(assets)
    flex = [i for i, l in enumerate(limbs) if l["dl"]]
    print(f"skeleton: {len(limbs)} limbs, {len(flex)} with display lists")
    env = TUNICS[args.tunic]
    bank = TextureBank(assets, args.src)

    # Body: every limb's own display list except the ones the bodygroups swap
    swapped = {limb for _, limb, _ in BODYGROUPS}
    runner = DLRunner(assets, flex, env)
    for i, l in enumerate(limbs):
        if l["dl"] and i not in swapped:
            runner.run(l["dl"], i)
    n = write_mesh_smd(os.path.join(args.src, "body.smd"), limbs, runner.tris, bank)
    print(f"body.smd: {n} triangles")

    groups = []
    for gname, limb, options in BODYGROUPS:
        subs = []
        for oname, dl in options:
            if assets.source(dl) is None:
                print(f"  {dl} missing, skipping {gname}/{oname}")
                continue
            r = DLRunner(assets, flex, env)
            r.run(dl, limb)
            fname = f"{gname}_{oname}.smd"
            n = write_mesh_smd(os.path.join(args.src, fname), limbs, r.tris, bank)
            subs.append(fname[:-4])
            print(f"{fname}: {n} triangles")
        groups.append((gname, subs))

    # Blinking: a skin family that swaps the eye texture (open / half / closed)
    eye_names = []
    eye_keys = [k for k in bank.made if k[0] == EYE_SKINS[0]]
    if eye_keys:
        proto = eye_keys[0]
        rng = proto[-1]
        for tex in EYE_SKINS:
            m = Material()
            (m.tex, m.mirror_s, m.mirror_t, m.tint, m.textured, m.alpha_tex, m.solid, m.chrome,
             m.clamp_s, m.clamp_t) = (tex,) + proto[1:-1]
            m.w = m.h = None
            eye_names.append(bank.name_for(m, rng)[0])

    # Animations
    anims = wanted_animations()
    if args.all_anims:
        have = sorted(f[len("gPlayerAnim_"):-len("_Data.inc.c")] for f in os.listdir(assets.anim_data)
                      if f.startswith("gPlayerAnim_") and f.endswith("_Data.inc.c"))
        names = [a for a, _ in anims]
        anims += [(h, False) for h in have if h not in names]
    seqs = []
    for name, loop in anims:
        frames = read_anim(assets, name)
        if not frames:
            print(f"  animation {name} not found")
            continue
        write_anim_smd(os.path.join(args.src, "anims", name + ".smd"), limbs, frames, args.keep_root_motion)
        seqs.append((name, loop, len(frames)))
    print(f"{len(seqs)} animations")

    # QC
    qc = [
        "// Generated by tools/zelda/make_link_model.py from your OoT ROM. Do not redistribute.",
        '$modelname "link.mdl"',
        '$cd "."',
        '$cdtexture "."',
        "$scale 1.0",
        # studiomdl turns every model 90 degrees (defaultzrotation, for 3ds Max's -Y forward);
        # these SMDs already face +X
        "$origin 0 0 0 -90",
        "$eyeposition 0 0 28",
            "",
        '$body "body" "body"',
    ]
    for gname, subs in groups:
        qc.append(f'$bodygroup "{gname}"')
        qc.append("{")
        for s in subs:
            qc.append(f'\tstudio "{s}"')
        qc.append("}")
    qc.append("")
    for name in sorted(bank.masked):
        qc.append(f'$texrendermode "{name}" masked')
    if eye_names:
        qc.append("")
        qc.append('$texturegroup "eyes"')
        qc.append("{")
        for e in eye_names:
            qc.append(f'\t{{ "{e}" }}')
        qc.append("}")
    qc.append("")
    for name, loop, count in seqs:
        # mstudioseqdesc_t labels are 32 bytes; zl_anim.cpp truncates the same way when looking up
        qc.append(f'$sequence "{name[:31]}" "anims/{name}" fps 20' + (" loop" if loop else ""))
    with open(os.path.join(args.src, "link.qc"), "w", newline="\n") as f:
        f.write("\n".join(qc) + "\n")
    print(f"wrote {os.path.join(args.src, 'link.qc')}")

    if args.preview:
        parts = ["body", "lhand_open", "rhand_open", "back_sword_shield"]
        shots = [(parts, None, 0)]
        for name, frame in (("link_normal_wait", 0), ("link_normal_run", 4), ("link_fighter_normal_kiru", 6),
                            ("link_normal_landing_roll", 6)):
            if any(n == name for n, _, _ in seqs):
                shots.append((parts, name, frame))
        preview(args.src, os.path.join(args.src, "preview.png"), shots)
        print(f"wrote {os.path.join(args.src, 'preview.png')}")

    smdl = find_studiomdl(args.studiomdl)
    if not smdl:
        print("studiomdl not found: compile link.qc yourself (studiomdl link.qc, or Crowbar), then copy link.mdl to",
              args.out)
        return
    print(f"compiling with {smdl}")
    res = subprocess.run([smdl, "link.qc"], cwd=args.src, capture_output=True, text=True)
    tail = (res.stdout + res.stderr).strip().split("\n")[-15:]
    print("\n".join(tail))
    mdl = os.path.join(args.src, "link.mdl")
    if res.returncode != 0 or not os.path.exists(mdl):
        sys.exit("studiomdl failed")
    os.makedirs(args.out, exist_ok=True)
    shutil.copyfile(mdl, os.path.join(args.out, "link.mdl"))
    print(f"wrote {os.path.join(args.out, 'link.mdl')}")


if __name__ == "__main__":
    main()
