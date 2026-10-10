link.mdl goes here. Build it from your own ROM with tools/zelda/make_link_model.py
(see docs/ZELDA_PORT.md, "Link's model"). zl_model already points here; without the file the
game uses models/player.mdl.

Sequence names in link.mdl are the OoT animation names without the gPlayerAnim_ prefix
(link_normal_wait, link_normal_landing_roll, link_fighter_Lside_kiru ...). dlls/zl_anim.cpp
finds them by name and plays them full body; anything missing falls back to the Half-Life
player.mdl stand-ins.
