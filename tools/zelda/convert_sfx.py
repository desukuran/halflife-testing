#!/usr/bin/env python3
"""
Zelda 64 port: prepare your own recordings of OoT sound effects (extract_sfx.py renders them from the ROM instead).

The game looks for sound/zelda/oot/<SFX NAME>.wav, one file per entry in
pm_shared/zl_sounds.h (NA_SE_PL_ROLL.wav, NA_SE_IT_SWORD_SWING.wav, ...).

Get the sounds from your own copy of the game: record them from an emulator, or use the
samples the decomp extracts with `make setup` (extracted/VERSION/assets/audio/samples). Name
each file after the SFX it is, put them in one folder, then:

    python3 convert_sfx.py --in my_oot_wavs --out ../../zelda/sound/zelda/oot

This converts every file to what GoldSrc can play (16 bit PCM, mono, 22050 Hz) and tells you
which ones are still missing. Input must be uncompressed PCM .wav (8/16/24/32 bit).

    python3 convert_sfx.py --list        prints the names the game wants, with the HL stand-in
"""

import argparse
import os
import re
import struct
import sys
import wave

try:
    import numpy as np
except ImportError:
    np = None


def read_sound_table():
    here = os.path.dirname(os.path.abspath(__file__))
    path = os.path.join(here, "..", "..", "pm_shared", "zl_sounds.h")
    src = open(path).read()
    return re.findall(r'\{\s*"([^"]+)",\s*"([^"]+)"\s*\}', src)


def load_wav(path):
    with wave.open(path, "rb") as w:
        ch, width, rate, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(n)
    if width == 1:
        data = np.frombuffer(raw, dtype=np.uint8).astype(np.float32) - 128.0
        data /= 128.0
    elif width == 2:
        data = np.frombuffer(raw, dtype="<i2").astype(np.float32) / 32768.0
    elif width == 3:
        b = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3)
        ints = (b[:, 0].astype(np.int32) | (b[:, 1].astype(np.int32) << 8) | (b[:, 2].astype(np.int32) << 16))
        ints = np.where(ints & 0x800000, ints - 0x1000000, ints)
        data = ints.astype(np.float32) / 8388608.0
    elif width == 4:
        data = np.frombuffer(raw, dtype="<i4").astype(np.float32) / 2147483648.0
    else:
        raise ValueError(f"unsupported sample width {width}")
    data = data.reshape(-1, ch).mean(axis=1)
    return data, rate


def resample(data, src, dst):
    if src == dst or len(data) == 0:
        return data
    n = int(round(len(data) * dst / src))
    x = np.linspace(0, len(data) - 1, n)
    return np.interp(x, np.arange(len(data)), data).astype(np.float32)


def save_wav(path, data, rate):
    pcm = np.clip(data * 32767.0, -32768, 32767).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(pcm.tobytes())


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--in", dest="inp")
    ap.add_argument("--out", default=os.path.join(here, "..", "..", "zelda", "sound", "zelda", "oot"))
    ap.add_argument("--rate", type=int, default=22050)
    ap.add_argument("--list", action="store_true")
    args = ap.parse_args()

    table = read_sound_table()
    wanted = sorted(set(oot for _, oot in table))

    if args.list or not args.inp:
        print(f"{'OoT sound (sound/zelda/oot/<name>.wav)':45s} Half-Life stand-in")
        for hl, oot in table:
            print(f"{oot:45s} {hl}")
        if not args.inp:
            return
        print()

    if np is None:
        sys.exit("needs numpy (pip install numpy)")

    os.makedirs(args.out, exist_ok=True)
    have = {}
    for f in os.listdir(args.inp):
        if f.lower().endswith(".wav"):
            have[os.path.splitext(f)[0].upper()] = os.path.join(args.inp, f)

    done = 0
    for name in wanted:
        src = have.get(name.upper())
        if not src:
            continue
        data, rate = load_wav(src)
        data = resample(data, rate, args.rate)
        peak = float(np.max(np.abs(data))) if len(data) else 0
        if peak > 1.0:
            data /= peak
        save_wav(os.path.join(args.out, name + ".wav"), data, args.rate)
        done += 1

    missing = [n for n in wanted if n.upper() not in have]
    print(f"converted {done}/{len(wanted)} into {args.out}")
    if missing:
        print("still missing (those play their Half-Life stand-in):")
        for n in missing:
            print("  " + n)


if __name__ == "__main__":
    main()
