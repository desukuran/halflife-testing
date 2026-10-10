OoT sound effects go here, one .wav per name in pm_shared/zl_sounds.h
(NA_SE_PL_ROLL.wav, NA_SE_IT_SWORD_SWING.wav ...), then set zl_sfx_oot 1.

They have to come from your own copy of the game. tools/zelda/convert_sfx.py converts a
folder of named .wav files to the format GoldSrc plays and lists what is still missing.
These files are git ignored; don't commit them.
