#!/usr/bin/env python3
"""
Zelda 64 port: build simple (unskinned) OoT objects into Half-Life models from YOUR ROM.

    python3 make_object_models.py --oot /path/to/oot [--studiomdl path] [--preview]

Same pipeline as make_link_model.py (the decomp's extracted display lists -> SMD + BMP + QC ->
studiomdl), for objects drawn as one piece. Each one gets a single bone and an "idle" sequence;
the game moves, spins and squashes them itself (squash and stretch through the entity's
fuser1 / fuser2, see StudioSetUpTransform).

  gossip_stone   object_gs: gGossipStoneMaterialDL, gGossipStoneDL, gGossipStoneSquishedDL
                 at EnGs's actor scale 0.1 -> models/zelda/gossip_stone.mdl
  treasure_chest object_box: gTreasureChestSkel with the front on limb 1 and the lid on limb 3
                 (EnBox_PostLimbDraw), sequences "idle" (shut) and "open"
                 (gTreasureChestAnim_00024C, the adult chest opening) at EnBox's scale 0.01
                 -> models/zelda/treasure_chest.mdl

  bomb           gameplay_keep: gBombCapDL and gBombBodyDL as EnBom_Draw draws them (the body
                 turned 90 degrees about X), at the bomb's scale 0.01. The body is a flat disc
                 the game turns to face the camera; its red flash (PRIMITIVE added to the
                 texture) becomes 8 skins -> models/zelda/bomb.mdl
  door           gameplay_keep: gDoorSkel (EnDoor), the door on limb 3 with both its faces
                 (gDoorLeftDL, gDoorRightDL), sequences "idle", "open_l" and "open_r"
                 (gDoorAdultOpeningLeftAnim / RightAnim) at the actor scale 0.01
                 -> models/zelda/door.mdl
  door_chains, door_lock
                 gameplay_dangeon_keep: the chains and padlock on a locked door (Actor_DrawDoorLock),
                 the chains with an "unlock" sequence -> models/zelda/door_chains.mdl, door_lock.mdl
  door_shutter   gameplay_keep: gDungeonDoorDL, Door_Shutter's generic dungeon door, at its
                 scale 1.0 -> models/zelda/door_shutter.mdl
  bush, bush_stump, grass
                 En_Kusa: gFieldBushDL (gameplay_field_keep), object_kusa's stump and grass tuft,
                 at its scale 0.4 -> models/zelda/bush.mdl, bush_stump.mdl, grass.mdl
  gi_*           the item Link holds over his head (sDrawItemTable in z_draw.c), each item's
                 display lists in its draw function's order, at Player_DrawGetItemImpl's
                 scale 0.2 -> models/zelda/gi_<item>.mdl

Skeletal objects use the decomp's standard animations (AnimationHeader: frame data plus joint
indices, where an index below staticIndexMax is a constant and the rest are one value a frame).

Nothing this writes may be redistributed: it comes from your ROM. The output is git ignored.
"""

import argparse
import os
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_link_model as L  # noqa: E402

OBJECTS = [
    {
        "name": "gossip_stone",
        "object": "object_gs",
        "scale": 0.1,                         # ICHAIN_VEC3F_DIV1000(scale, 100) in z_en_gs.c
        "dls": ["gGossipStoneMaterialDL", "gGossipStoneDL", "gGossipStoneSquishedDL"],
    },
    {
        "name": "treasure_chest",
        "object": "object_box",
        "scale": 0.01,                        # Actor_SetScale( 0.01f ) for the big chests
        "limbs": "gTreasureChestLimbs",
        "bones": ["root", "body", "lid", "lid_end"],
        "limb_dls": {0: ["gTreasureChestChestFrontDL"], 2: ["gTreasureChestChestSideAndLidDL"]},
        "env": (0, 0, 0),                     # EnBox_Draw: gDPSetEnvColor( 0, 0, 0, 255 )
        "anims": [("idle", "gTreasureChestAnim_00024C", 0, True), ("open", "gTreasureChestAnim_00024C", None, False)],
    },
    {
        "name": "bomb",
        "object": "gameplay_keep",
        "scale": 0.01,                        # Actor_SetScale( 0.01f ) once the bomb is out
        "dls": ["gBombCapDL", "gBombBodyDL"],
        "rot_x90": ["gBombBodyDL"],           # Matrix_RotateZYX( 0x4000, 0, 0 ) before the body
        "flash": "gBombBodyDL",
    },
    # En_Kusa at its scale 0.4 (ICHAIN_VEC3F_DIV1000(scale, 400)): the bush (type 0), what's
    # left of it once cut, and the tuft of grass (types 1 and 2)
    {"name": "bush", "object": "gameplay_field_keep", "scale": 0.4, "dls": ["gFieldBushDL"]},
    {"name": "bush_stump", "object": "object_kusa", "scale": 0.4, "dls": ["object_kusa_DL_0002E0"]},
    {"name": "grass", "object": "object_kusa", "scale": 0.4, "dls": ["object_kusa_DL_000140"]},
    # EnDoor at the actor's default scale 0.01: gDoorSkel with the door on limb 3, both faces
    # (EnDoor_OverrideLimbDraw picks the side facing the camera; each face is one sided, so both
    # can be drawn), sequences "idle" (shut), "open_l" and "open_r" (the adult ones)
    {
        "name": "door",
        "object": "gameplay_keep",
        "scale": 0.01,
        "limbs": "gDoorLimbs",
        "bones": ["root", "hinge", "turn", "door"],
        "limb_dls": {3: ["gDoorLeftDL", "gDoorRightDL"]},
        "anims": [("idle", "gDoorAdultOpeningLeftAnim", 0, True),
                  ("open_l", "gDoorAdultOpeningLeftAnim", None, False),
                  ("open_r", "gDoorAdultOpeningRightAnim", None, False)],
    },
    # Door_Shutter's DOORSHUTTER_GFX_GENERIC at its scale 1.0: the dungeon door that slides up
    {"name": "door_shutter", "object": "gameplay_keep", "scale": 1.0, "dls": ["gDungeonDoorDL"]},
    # Obj_Syokudai's stands at its scale 1.0 (the flame is drawn by the game, zl_effects.cpp)
    {"name": "torch_golden", "object": "object_syokudai", "scale": 1.0, "dls": ["gGoldenTorchDL"]},
    {"name": "torch_timed", "object": "object_syokudai", "scale": 1.0, "dls": ["gTimedTorchDL"]},
    {"name": "torch_wooden", "object": "object_syokudai", "scale": 1.0, "dls": ["gWoodenTorchDL"]},
]

# sDrawItemTable entries, display lists in the order their GetItem_Draw* function draws them
# (the *ColorDL ones only set colours for the mesh after them)
GI_ITEMS = [
    ("gi_bow", ["gGiBowDL"]),                                   # GID_BOW, DrawOpa0
    ("gi_hookshot", ["gGiHookshotDL"]),
    ("gi_longshot", ["gGiLongshotDL"]),
    ("gi_bombbag", ["gGiBombBag20BagColorDL", "gGiBombBagDL", "gGiBombBag20RingColorDL", "gGiBombBagRingDL"]),  # DrawOpa1023
    ("gi_heart_container", ["gGiHeartBorderDL", "gGiHeartContainerDL"]),  # DrawXlu01
    ("gi_heart", ["gGiRecoveryHeartDL"]),
    ("gi_key", ["gGiSmallKeyDL"]),
    ("gi_sword", ["gGiKokiriSwordDL"]),                         # OoT has no get-item Master Sword
    ("gi_shield_hylian", ["gGiHylianShieldDL"]),
    ("gi_magic_small", ["gGiMagicJarSmallDL"]),
    ("gi_magic_large", ["gGiMagicJarLargeDL"]),
    ("gi_arrows", ["gGiArrowMediumDL"]),
    ("gi_bomb", ["gGiBombDL"]),
    ("gi_nuts", ["gGiNutDL"]),
    # DrawSmallRupee / DrawOpa10Xlu32: inner colour, inner, outer colour, outer
    ("gi_rupee_green", ["gGiGreenRupeeInnerColorDL", "gGiRupeeInnerDL", "gGiGreenRupeeOuterColorDL", "gGiRupeeOuterDL"]),
    ("gi_rupee_blue", ["gGiBlueRupeeInnerColorDL", "gGiRupeeInnerDL", "gGiBlueRupeeOuterColorDL", "gGiRupeeOuterDL"]),
    ("gi_rupee_red", ["gGiRedRupeeInnerColorDL", "gGiRupeeInnerDL", "gGiRedRupeeOuterColorDL", "gGiRupeeOuterDL"]),
    ("gi_rupee_purple", ["gGiPurpleRupeeInnerColorDL", "gGiRupeeInnerDL", "gGiPurpleRupeeOuterColorDL", "gGiRupeeOuterDL"]),
]
for _name, _dls in GI_ITEMS:
    OBJECTS.append({"name": _name, "object": None, "scale": 0.2, "dls": _dls})

FLASH_LEVELS = 8        # EnBom flashIntensity 0..140 in this many steps


def find_object(extracted, dl_name):
    """The object folder a display list was extracted into."""
    root = os.path.join(extracted, "assets", "objects")
    for d in sorted(os.listdir(root)):
        if os.path.exists(os.path.join(root, d, dl_name + ".inc.c")):
            return d
    return None


def make_flash_skins(src, bmp, levels):
    """EnBom_Draw: colour = PRIMITIVE + TEXEL0 with PRIMITIVE (flashIntensity, 0, 40). Returns
    the skin names, the plain one (intensity 0) first."""
    from PIL import Image
    img = Image.open(os.path.join(src, bmp))
    mask = [i == 255 for i in img.getdata()]
    rgb = img.convert("RGB")
    names = []
    for k in range(levels):
        flash = round(140 * k / (levels - 1))
        out = Image.new("RGBA", rgb.size)
        px = [(min(255, r + flash), g, min(255, b + 40), 0 if mask[i] else 255)
              for i, (r, g, b) in enumerate(rgb.getdata())]
        out.putdata(px)
        name = bmp if k == 0 else bmp[:-4] + f"_f{k}.bmp"
        L.save_bmp8(out, os.path.join(src, name), True)
        names.append(name)
    return names


class ObjectAssets(L.Assets):
    def __init__(self, extracted, obj):
        super().__init__(extracted)
        self.obj = os.path.join(extracted, "assets", "objects", obj)
        self.search = [self.obj, self.keep]
        self._listing = {d: os.listdir(d) for d in self.search if os.path.isdir(d)}


def parse_limbs(assets, listing_name):
    """StandardLimb list -> limbs with joint position, parent and no display lists."""
    import re
    names = re.findall(r"&(\w+)", assets.source(listing_name))
    limbs = []
    for name in names:
        src = assets.source(name)
        joint = [int(v) for v in re.findall(r"(-?\d+),\s*//\s*[xyz]\b", src)[:3]]
        child = re.search(r"/\*\s*(\d+)\s*\*/[^\n]*//\s*child", src)
        sib = re.search(r"/\*\s*(\d+)\s*\*/[^\n]*//\s*sibling", src)
        limbs.append({"name": name, "joint": joint, "dl": None,
                      "child": int(child.group(1)) if child else 255,
                      "sibling": int(sib.group(1)) if sib else 255})
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


def read_standard_anim(assets, name, limb_count):
    """Frames of an AnimationHeader: list of (root_pos, [rot x limbs], 0)."""
    import re
    hdr = assets.source(name)
    count = int(re.search(r"(\d+),\s*//\s*frameCount", hdr).group(1))
    data_name = re.search(r"(\w+),\s*//\s*frameData", hdr).group(1)
    idx_name = re.search(r"(\w+),\s*//\s*jointIndices", hdr).group(1)
    static_max = int(re.search(r"(\d+),\s*//\s*staticIndexMax", hdr).group(1))
    data = [int(v, 0) for v in re.findall(r"(-?0x[0-9A-Fa-f]+|-?\d+),\s*//", assets.source(data_name))]
    idx = [int(v) for v in re.findall(r"(\d+),\s*//\s*[xyz]\b", assets.source(idx_name))]
    joints = [idx[i:i + 3] for i in range(0, len(idx), 3)]

    def value(i, frame):
        v = data[i] if i < static_max else data[i + frame]
        return v - 0x10000 if v >= 0x8000 else v

    frames = []
    for f in range(count):
        vals = [[value(i, f) for i in j] for j in joints[:limb_count + 1]]
        rots = [tuple(c * L.BINANG for c in v) for v in vals[1:]]
        frames.append((vals[0], rots, 0))
    return frames


def build(obj, extracted, src_root, out_dir, studiomdl, preview):
    L.Material.FULL = obj["name"].startswith("gi_")    # their colours are combiner blends
    if not obj["object"]:
        obj = dict(obj, object=find_object(extracted, obj["dls"][-1]))
        if not obj["object"]:
            print(f"{obj['name']}: {obj['dls'][-1]} not extracted")
            return False
    assets = ObjectAssets(extracted, obj["object"])
    if not os.path.isdir(assets.obj):
        print(f"{obj['name']}: {assets.obj} not extracted")
        return False

    src = os.path.join(src_root, obj["name"])
    os.makedirs(os.path.join(src, "anims"), exist_ok=True)

    # OoT units x actor scale x 72/68, Y up -> Z up
    L.SCALE = obj["scale"] * 72.0 / 68.0
    L.FEET_Z = 0.0
    runner = L.DLRunner(assets, [0], obj.get("env", (255, 255, 255)))

    flash_tris = None
    if "limbs" in obj:
        limbs = parse_limbs(assets, obj["limbs"])
        L.BONE_NAMES = obj["bones"]
        for limb, dls in sorted(obj["limb_dls"].items()):
            runner.run(dls[0], limb)
            for dl in dls[1:]:
                runner._run(dl, 0)
        sequences = []
        for seq, anim, frame, loop in obj["anims"]:
            frames = read_standard_anim(assets, anim, len(limbs))
            if frame is not None:
                frames = [frames[frame]]
            sequences.append((seq, frames, loop))
    else:
        # One bone at the feet
        L.BONE_NAMES = ["root"]
        limbs = [{"name": "root", "joint": [0, 0, 0], "parent": -1, "child": 255, "sibling": 255, "dl": None}]
        for i, dl in enumerate(obj["dls"]):
            first = len(runner.tris)
            if i == 0:
                runner.run(dl, 0)
            else:
                runner._run(dl, 0)      # the vertex cache carries over between them, like on the N64
            if dl in obj.get("rot_x90", ()):
                # Rx(90 degrees): (x, y, z) -> (x, -z, y), positions and normals
                for t in range(first, len(runner.tris)):
                    mat, vs, two = runner.tris[t]
                    vs = [(v[0], (v[1][0], -v[1][2], v[1][1]), (v[2][0], -v[2][2], v[2][1])) + tuple(v[3:]) for v in vs]
                    runner.tris[t] = (mat, vs, two)
            if dl == obj.get("flash"):
                # PRIMITIVE + TEXEL0 isn't a plain tint: keep the texture, the skins add the colour
                flash_tris = range(first, len(runner.tris))
                for t in flash_tris:
                    mat = runner.tris[t][0]
                    mat.textured, mat.solid, mat.alpha_tex = True, False, True
                    mat.tex, mat.tint = runner.tex, (255, 255, 255)
                    mat.mirror_s, mat.mirror_t = "G_TX_MIRROR" in runner.cms, "G_TX_MIRROR" in runner.cmt
        sequences = [("idle", [([0, 0, 0], [(0.0, 0.0, 0.0)], 0)], True)]

    bank = L.TextureBank(assets, src)
    n = L.write_mesh_smd(os.path.join(src, "body.smd"), limbs, runner.tris, bank)
    for seq, frames, loop in sequences:
        L.write_anim_smd(os.path.join(src, "anims", f"{seq}.smd"), limbs, frames, True)
    print(f"{obj['name']}: {n} triangles, {len(sequences)} sequences")

    qc = [
        "// Generated by tools/zelda/make_object_models.py from your OoT ROM. Do not redistribute.",
        f'$modelname "{obj["name"]}.mdl"',
        '$cd "."',
        '$cdtexture "."',
        "$scale 1.0",
        "$origin 0 0 0 -90",                 # cancel studiomdl's default 90 degree turn
        '$body "body" "body"',
    ]
    skins = None
    if obj.get("flash") and flash_tris:
        mat = runner.tris[flash_tris[0]][0]
        body_bmp = [info[0] for key, info in bank.made.items() if key[0] == mat.tex][0]
        skins = make_flash_skins(src, body_bmp, FLASH_LEVELS)
        bank.masked.update(skins)
    for t in sorted(bank.masked):
        qc.append(f'$texrendermode "{t}" masked')
    if skins:
        qc.append("$texturegroup flash")
        qc.append("{")
        qc += ['{ "%s" }' % k for k in skins]
        qc.append("}")
    for seq, frames, loop in sequences:
        qc.append(f'$sequence "{seq}" "anims/{seq}" fps 20' + (" loop" if loop else ""))
    with open(os.path.join(src, f"{obj['name']}.qc"), "w", newline="\n") as f:
        f.write("\n".join(qc) + "\n")

    if preview:
        L.preview(src, os.path.join(src, "preview.png"), [(["body"], None, 0)])

    if not studiomdl:
        print(f"  studiomdl not found: compile {os.path.join(src, obj['name'] + '.qc')} yourself")
        return True
    res = subprocess.run([studiomdl, f"{obj['name']}.qc"], cwd=src, capture_output=True, text=True)
    mdl = os.path.join(src, f"{obj['name']}.mdl")
    if res.returncode != 0 or not os.path.exists(mdl):
        print("\n".join((res.stdout + res.stderr).strip().split("\n")[-10:]))
        return False
    os.makedirs(out_dir, exist_ok=True)
    shutil.copyfile(mdl, os.path.join(out_dir, f"{obj['name']}.mdl"))
    print(f"  wrote {os.path.join(out_dir, obj['name'] + '.mdl')}")
    return True


# Actor_DrawDoorLock's DOORLOCK_NORMAL (sDoorLocksInfo): chain angle, chain length, how far up the
# door the lock sits, in the 0.01 scaled space the doors draw it in
LOCK_CHAIN_ANGLE = 0.54
LOCK_CHAIN_LENGTH = 6000.0
LOCK_FRAMES = 10            # lockTimer: 10 locked .. 0 open


def build_door_lock(extracted, src_root, out_dir, studiomdl):
    """door_chains.mdl: the four chains (gDoorChainDL) across one face of a locked door, a bone each,
    with "idle" (locked) and "unlock", where they slide off along their length over 10 frames like
    Actor_DrawDoorLock's frame 10 -> 0. door_lock.mdl: the padlock (gDoorLockDL), which the game
    shrinks away (its scale is frame / 10). Both centred on the lock, facing +X, in Door_Shutter's
    space (Matrix_Scale( 0.01, 0.01, 0.025 )); the game puts one of each on both faces."""
    import math
    assets = ObjectAssets(extracted, "gameplay_dangeon_keep")
    if not os.path.isdir(assets.obj):
        print(f"door_lock: {assets.obj} not extracted")
        return False
    L.Material.FULL = False
    L.SCALE = 72.0 / 68.0
    L.FEET_Z = 0.0
    a = LOCK_CHAIN_ANGLE
    rots, r = [], 0.0
    for i in range(4):
        rots.append(r)
        r += 2.0 * a if i % 2 else math.pi - 2.0 * a

    def place(tris, first, fn):
        for t in range(first, len(tris)):
            mat, vs, two = tris[t]
            vs = [(v[0], fn(v[0], v[1]), v[2]) + tuple(v[3:]) for v in vs]
            tris[t] = (mat, vs, two)

    def rotz(rz, x, y):
        return x * math.cos(rz) - y * math.sin(rz), x * math.sin(rz) + y * math.cos(rz)

    def finish(name, limbs, runner, anims):
        src = os.path.join(src_root, name)
        os.makedirs(os.path.join(src, "anims"), exist_ok=True)
        bank = L.TextureBank(assets, src)
        n = L.write_mesh_smd(os.path.join(src, "body.smd"), limbs, runner.tris, bank)
        qc = ["// Generated by tools/zelda/make_object_models.py from your OoT ROM. Do not redistribute.",
              f'$modelname "{name}.mdl"', '$cd "."', '$cdtexture "."', "$scale 1.0", "$origin 0 0 0 -90",
              '$body "body" "body"']
        for t in sorted(bank.masked):
            qc.append(f'$texrendermode "{t}" masked')
        for seq, frames in anims:
            lines = L.smd_header(limbs) + ["skeleton"]
            for k, offs in enumerate(frames):
                lines.append(f"time {k}")
                lines.append(L.bone_frame(limbs, [(0.0, 0.0, 0.0)] * len(limbs), [0, 0, 0])[0])
                for i, (dx, dy) in enumerate(offs):
                    lines.append(f"{i + 1} {dx * L.SCALE:.6f} {dy * L.SCALE:.6f} 0.000000 0.000000 0.000000 0.000000")
            lines.append("end")
            with open(os.path.join(src, "anims", seq + ".smd"), "w", newline="\n") as f:
                f.write("\n".join(lines) + "\n")
            qc.append(f'$sequence "{seq}" "anims/{seq}" fps 20' + (" loop" if seq == "idle" else ""))
        with open(os.path.join(src, f"{name}.qc"), "w", newline="\n") as f:
            f.write("\n".join(qc) + "\n")
        print(f"{name}: {n} triangles")
        if not studiomdl:
            print(f"  studiomdl not found: compile {os.path.join(src, name + '.qc')} yourself")
            return True
        res = subprocess.run([studiomdl, f"{name}.qc"], cwd=src, capture_output=True, text=True)
        mdl = os.path.join(src, f"{name}.mdl")
        if res.returncode != 0 or not os.path.exists(mdl):
            print("\n".join((res.stdout + res.stderr).strip().split("\n")[-10:]))
            return False
        os.makedirs(out_dir, exist_ok=True)
        shutil.copyfile(mdl, os.path.join(out_dir, f"{name}.mdl"))
        print(f"  wrote {os.path.join(out_dir, name + '.mdl')}")
        return True

    def bone(name, parent):
        return {"name": name, "joint": [0, 0, 0], "parent": parent, "child": 255, "sibling": 255, "dl": None}

    # The chains: Matrix_RotateZ( chainRotZ ), then out along their length
    runner = L.DLRunner(assets, [0], (255, 255, 255))
    for i, rz in enumerate(rots):
        first = len(runner.tris)
        runner.run("gDoorChainDL", i + 1)
        place(runner.tris, first, lambda limb, p, rz=rz: (*(c * 0.01 for c in rotz(rz, p[0], p[1])), p[2] * 0.025))
    limbs = [bone("root", -1)] + [bone(f"chain{i}", 0) for i in range(4)]
    L.BONE_NAMES = [l["name"] for l in limbs]
    frames = []
    for f in range(LOCK_FRAMES, -1, -1):
        tx = -(LOCK_FRAMES - f) * math.sin(a) * 0.1 * LOCK_CHAIN_LENGTH
        ty = (LOCK_FRAMES - f) * math.cos(a) * 0.1 * LOCK_CHAIN_LENGTH
        frames.append([tuple(c * 0.01 for c in rotz(rz, tx, ty)) for rz in rots])
    ok = finish("door_chains", limbs, runner, [("idle", frames[:1]), ("unlock", frames)])

    # The padlock
    runner = L.DLRunner(assets, [0], (255, 255, 255))
    runner.run("gDoorLockDL", 0)
    place(runner.tris, 0, lambda limb, p: (p[0] * 0.01, p[1] * 0.01, p[2] * 0.025))
    limbs = [bone("root", -1)]
    L.BONE_NAMES = ["root"]
    ok &= finish("door_lock", limbs, runner, [("idle", [[]])])
    return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--oot", required=True)
    ap.add_argument("--version", default="ntsc-1.0")
    ap.add_argument("--src", default=os.path.join(L.REPO, "tools", "zelda", "build"))
    ap.add_argument("--out", default=os.path.join(L.REPO, "zelda", "models", "zelda"))
    ap.add_argument("--studiomdl")
    ap.add_argument("--preview", action="store_true")
    ap.add_argument("--only", nargs="*")
    args = ap.parse_args()

    extracted = os.path.join(args.oot, "extracted", args.version)
    smdl = L.find_studiomdl(args.studiomdl)
    ok = True
    for obj in OBJECTS:
        if args.only and obj["name"] not in args.only:
            continue
        ok &= build(obj, extracted, args.src, args.out, smdl, args.preview)
    if not args.only or "door_lock" in args.only:
        ok &= build_door_lock(extracted, args.src, args.out, smdl)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
