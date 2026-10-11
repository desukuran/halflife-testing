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
]


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
        runner.run(obj["dls"][0], 0)
        for dl in obj["dls"][1:]:
            runner._run(dl, 0)          # the vertex cache carries over between them, like on the N64
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
    for t in sorted(bank.masked):
        qc.append(f'$texrendermode "{t}" masked')
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
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
