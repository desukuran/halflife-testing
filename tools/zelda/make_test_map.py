#!/usr/bin/env python3
"""
Zelda 64 port: writes zelda/maps/zl_test.map, a small test room in Valve 220 format.

Compile it with your usual tools (J.A.C.K. / Hammer + VHLT):
    hlcsg zl_test && hlbsp zl_test && hlvis zl_test && hlrad zl_test

What's in it:
  - ledges at 40 / 64 / 100 units: the first two can be climbed (push into them or press A),
    the last one can't (OoT's limit is 79.4 units, ZL_CLIMB_MAX)
  - a gap with a high platform across it and a wooden wall to hookshot to
  - two zombies and two headcrabs to Z-target, roll past and sword
  - a scientist and a guard to talk to (they'll follow you if you ask)
  - a chest with the Fairy Bow, a Hookshot on a pedestal, a Bomb Bag, rupees and hearts
  - breakable crates for bombs, a sign, Navi's hint on spawn
  - a door to open (zl_door) and a locked dungeon door that slides up (zl_door_shutter) into a
    little room with a chest; its small key is by the bow chest
  - ambient_zmusic playing music/oot/test.miniusf (put any rip there, or a .wav)

Textures default to names from halflife.wad; change TEX_* if your editor complains.
"""

import os

TEX_FLOOR = "GENERIC015V"
TEX_WALL = "GENERIC028"
TEX_CEIL = "GENERIC015V"
TEX_LEDGE = "CRATE01"
TEX_WOOD = "TRRM_WOOD"       # halflife.wad; "wood" in the name keeps it hookable
TEX_CRATE = "CRATE04"
TEX_SIGN = "WOOD_SIGN"


def face(p1, p2, p3, tex, axis):
    if axis == "z":
        u, v = "[ 1 0 0 0 ]", "[ 0 -1 0 0 ]"
    elif axis == "x":
        u, v = "[ 0 1 0 0 ]", "[ 0 0 -1 0 ]"
    else:
        u, v = "[ 1 0 0 0 ]", "[ 0 0 -1 0 ]"
    f = lambda p: "( %d %d %d )" % p
    return f"{f(p1)} {f(p2)} {f(p3)} {tex} {u} {v} 0 1 1\n"


def box(mins, maxs, tex, top=None):
    x0, y0, z0 = mins
    x1, y1, z1 = maxs
    t = top or tex
    s = "{\n"
    s += face((x0, y0, z0), (x0, y0 + 1, z0), (x0, y0, z0 + 1), tex, "x")
    s += face((x0, y0, z0), (x0, y0, z0 + 1), (x0 + 1, y0, z0), tex, "y")
    s += face((x0, y0, z0), (x0 + 1, y0, z0), (x0, y0 + 1, z0), tex, "z")
    s += face((x1, y1, z1), (x1, y1 + 1, z1), (x1 + 1, y1, z1), t, "z")
    s += face((x1, y1, z1), (x1 + 1, y1, z1), (x1, y1, z1 + 1), tex, "y")
    s += face((x1, y1, z1), (x1, y1, z1 + 1), (x1, y1 + 1, z1), tex, "x")
    s += "}\n"
    return s


def entity(classname, keys=None, brushes=None):
    s = "{\n" + f'"classname" "{classname}"\n'
    for k, v in (keys or {}).items():
        s += f'"{k}" "{v}"\n'
    for b in brushes or []:
        s += b
    s += "}\n"
    return s


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out = os.path.join(here, "..", "..", "zelda", "maps", "zl_test.map")

    W, D, H = 1536, 1024, 384      # room size
    T = 16                          # wall thickness
    world = []

    # Shell. Walls go down to the bottom of the pit on the east side.
    PIT = -256
    world.append(box((-T, -T, PIT - T), (1152, D + T, 0), TEX_FLOOR))            # main floor, solid down to the pit
    world.append(box((1152, -T, PIT - T), (W + T, D + T, PIT), TEX_FLOOR))       # pit floor
    world.append(box((-T, -T, H), (W + T, D + T, H + T), TEX_CEIL))              # ceiling
    world.append(box((-T, -T, PIT), (0, D + T, H), TEX_WALL))                    # west
    world.append(box((W, -T, PIT), (W + T, D + T, H), TEX_WALL))                 # east
    world.append(box((0, -T, PIT), (W, 0, H), TEX_WALL))                         # south
    # North, with a doorway into a little dungeon room for the sliding door (Door_Shutter: 80 x 110
    # OoT units, 85 x 117 here)
    world.append(box((0, D, PIT), (300, D + T, H), TEX_WALL))                    # north
    world.append(box((388, D, PIT), (W, D + T, H), TEX_WALL))
    world.append(box((300, D, 120), (388, D + T, H), TEX_WALL))

    # The dungeon room behind it
    world.append(box((184, D + T, -16), (516, 1312, 0), TEX_FLOOR))
    world.append(box((184, D + T, 200), (516, 1312, 216), TEX_CEIL))
    world.append(box((184, D + T, 0), (200, 1312, 200), TEX_WALL))
    world.append(box((500, D + T, 0), (516, 1312, 200), TEX_WALL))
    world.append(box((200, 1296, 0), (500, 1312, 200), TEX_WALL))

    # Ledges along the south wall: 40 (step up), 64 (climb), 100 (too high)
    world.append(box((96, 0, 0), (224, 96, 40), TEX_LEDGE))
    world.append(box((288, 0, 0), (416, 96, 64), TEX_LEDGE))
    world.append(box((480, 0, 0), (608, 96, 100), TEX_LEDGE))

    # An opening in the wall to climb into: 48 up, 96 tall, 96 deep
    world.append(box((704, 0, 0), (800, 96, 48), TEX_LEDGE))
    world.append(box((704, 0, 144), (800, 96, H), TEX_LEDGE))

    # A gap along the east side with a high platform across it and a wooden wall to hookshot
    world.append(box((1408, 640, PIT), (1536, 1024, 160), TEX_LEDGE))            # high platform
    world.append(box((1528, 700, 160), (1536, 960, 320), TEX_WOOD))              # hookshot target

    # Divider with a doorway for a door (EnDoor: 60 x 100 OoT units, 64 x 106 here)
    world.append(box((640, 640, 0), (656, 736, H), TEX_WALL))
    world.append(box((640, 800, 0), (656, 1024, H), TEX_WALL))
    world.append(box((640, 736, 112), (656, 800, H), TEX_WALL))

    # Pedestal for the hookshot
    world.append(box((832, 832, 0), (896, 896, 32), TEX_LEDGE))

    ents = []
    ents.append(entity("worldspawn", {
        "wad": "\\half-life\\valve\\halflife.wad;\\half-life\\valve\\liquids.wad;\\half-life\\valve\\xeno.wad",
        "message": "Zelda 64 test room",
        "skyname": "desert",
        "mapversion": "220",
    }, world))

    ents.append(entity("info_player_start", {"origin": "128 512 36", "angles": "0 0 0"}))
    ents.append(entity("light", {"origin": "384 512 352", "_light": "255 240 220 300"}))
    ents.append(entity("light", {"origin": "1024 512 352", "_light": "255 240 220 300"}))
    ents.append(entity("light", {"origin": "1344 512 0", "_light": "200 220 255 200"}))
    ents.append(entity("light", {"origin": "896 896 300", "_light": "255 255 200 200"}))

    # Navi's hint once you spawn
    ents.append(entity("trigger_auto", {"origin": "64 64 64", "target": "navi_hint", "delay": "1.5", "spawnflags": "1"}))
    ents.append(entity("trigger_zmessage", {"origin": "64 96 64", "targetname": "navi_hint", "message": "NAVI_HEY", "spawnflags": "1"}))

    # Music: any .miniusf rip (or .wav) you drop in zelda/music/oot/test.miniusf
    ents.append(entity("ambient_zmusic", {"origin": "64 128 64", "track": "music/oot/test.miniusf",
                                          "fadein": "1", "fadeout": "2", "volume": "10", "spawnflags": "1"}))

    # A Gossip Stone: read it, hit it, bomb it
    ents.append(entity("monster_gossip_stone", {"origin": "256 400 0", "angles": "0 0 0"}))

    # Sign by the start
    ents.append(entity("zl_sign", {"origin": "192 640 0", "message": "SIGN_ZL_TEST"}))

    # NPCs
    ents.append(entity("monster_scientist", {"origin": "320 760 0", "angles": "0 180 0", "body": "-1"}))
    ents.append(entity("monster_barney", {"origin": "448 820 0", "angles": "0 200 0"}))

    # Enemies on the far side of the door
    ents.append(entity("monster_zombie", {"origin": "1000 300 0", "angles": "0 180 0"}))
    ents.append(entity("monster_zombie", {"origin": "1060 520 0", "angles": "0 180 0"}))
    ents.append(entity("monster_headcrab", {"origin": "900 200 0", "angles": "0 180 0"}))
    ents.append(entity("monster_headcrab", {"origin": "940 420 0", "angles": "0 180 0"}))

    # Items
    ents.append(entity("zl_chest", {"origin": "512 900 0", "angles": "0 270 0", "item": "item_zl_bow"}))
    ents.append(entity("item_zl_hookshot", {"origin": "864 864 40"}))
    ents.append(entity("item_zl_bombbag", {"origin": "352 48 72"}))
    for i, (x, y) in enumerate([(160, 48), (544, 48), (700, 300), (720, 360), (740, 420)]):
        z = 48 if i == 0 else (108 if i == 1 else 8)
        ents.append(entity("item_zl_rupee" if i < 3 else "item_zl_rupee_blue", {"origin": f"{x} {y} {z}"}))
    ents.append(entity("item_zl_heart", {"origin": "1460 820 168"}))

    # Standing torches (Obj_Syokudai): lit golden and wooden ones, and an unlit timed one
    ents.append(entity("zl_torch", {"origin": "320 300 0", "type": "0"}))
    ents.append(entity("zl_torch", {"origin": "480 300 0", "type": "2"}))
    ents.append(entity("zl_torch", {"origin": "560 420 0", "type": "1", "spawnflags": "1"}))

    # Grass and bushes to cut (En_Kusa)
    for x, y in [(100, 880), (150, 940), (200, 880), (250, 960)]:
        ents.append(entity("zl_grass", {"origin": f"{x} {y} 0", "type": "1"}))
    for x, y in [(400, 560), (470, 600)]:
        ents.append(entity("zl_grass", {"origin": f"{x} {y} 0", "type": "0"}))
    ents.append(entity("item_zl_rupee_red", {"origin": "1480 900 168"}))
    ents.append(entity("item_zl_heartcontainer", {"origin": "1460 700 168"}))

    # Breakable crates (bombs / sword)
    for x, y in [(760, 760), (760, 820)]:
        ents.append(entity("func_breakable", {"material": "1", "health": "20", "spawnobject": "0"},
                           [box((x, y, 0), (x + 48, y + 48, 48), TEX_CRATE)]))

    # The door in the divider (En_Door): face it and press A
    ents.append(entity("zl_door", {"origin": "648 768 0", "angles": "0 0 0"}))

    # The dungeon door (Door_Shutter), locked: the small key is by the chest
    ents.append(entity("zl_door_shutter", {"origin": "344 1032 0", "angles": "0 90 0", "spawnflags": "1"}))
    ents.append(entity("item_zl_key", {"origin": "580 900 8"}))
    ents.append(entity("light", {"origin": "344 1160 180", "_light": "255 200 150 150"}))
    ents.append(entity("zl_torch", {"origin": "240 1250 0", "type": "0"}))
    ents.append(entity("zl_torch", {"origin": "450 1250 0", "type": "0"}))
    ents.append(entity("zl_chest", {"origin": "344 1220 0", "angles": "0 270 0", "item": "item_zl_rupee_red"}))

    with open(out, "w", newline="\n") as f:
        f.write("// Generated by tools/zelda/make_test_map.py\n")
        for e in ents:
            f.write(e)
    print(f"wrote {os.path.normpath(out)}")


if __name__ == "__main__":
    main()
