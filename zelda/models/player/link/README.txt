link.mdl goes here once it is built from your ROM (see docs/ZELDA_PORT.md, "Link's model").
Then: zl_model "models/player/link/link.mdl" and restart the map.

Sequence names in link.mdl are the OoT animation names without the gPlayerAnim_ prefix
(link_normal_wait, link_normal_landing_roll, link_fighter_Lside_kiru ...). dlls/zl_anim.cpp
finds them by name and plays them full body; anything missing falls back to the Half-Life
player.mdl stand-ins.
