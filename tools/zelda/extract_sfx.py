#!/usr/bin/env python3
"""
Zelda 64 port: render OoT's sound effects to sound/zelda/oot/<NA_SE_NAME>.wav from YOUR ROM.

OoT has no sound effect files: every NA_SE_* is a little program in the sound effects
sequence (seq 0, assets/audio/sequences/seq_0.prg.seq in the decomp) that loads instrument
samples, plays notes, slides pitch and so on. This script runs those programs (a best-effort
interpreter of the commands seq 0 uses) over the samples `make setup` extracted, and writes
16 bit mono 22050 Hz WAVs GoldSrc can play.

    python3 extract_sfx.py --oot /path/to/oot [--version ntsc-1.0] [--all]

  * by default only the sounds pm_shared/zl_sounds.h uses; --all renders every NA_SE_*
  * Link's voice clips have several takes (NA_SE_VO_LI_SWORD_N has four "Hup!"s). They are
    written as NAME.wav, NAME_2.wav, ... and listed in variants.txt; the game picks one at
    random each time like OoT does
  * the random pitch OoT adds per sound (the sfx table's rand field) is applied by the game,
    not baked in

What's approximated: envelopes (attack/decay are flat, release is a short fade), vibrato,
filters, reverb and panning are ignored. Sounds that
loop until the game stops them (charging, hookshot chain) are cut at --max-seconds.

Nothing this writes may be redistributed: it comes from your ROM. The output folder is git
ignored.
"""

import argparse
import math
import os
import random
import re
import struct
import sys
import wave
import xml.etree.ElementTree as ET

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))

OUT_RATE = 22050
TICKS_PER_SEC = 120 * 48 / 60.0    # seq 0: tempo 120, 48 tatums a beat
NOTE_NAMES = ["A", "BF", "B", "C", "DF", "D", "EF", "E", "F", "GF", "G", "AF"]
INSTR_SFX = 0x7E                   # FONTANY_INSTR_SFX: the note picks an Effect
INSTR_DRUM = 0x7F                  # FONTANY_INSTR_DRUM: the note picks a Drum
# FONTANY_INSTR_SAWTOOTH .. ASM_NOISE (128..136): the audio library's built-in waveforms
FONTANY_WAVES = ["SAWTOOTH", "TRIANGLE", "SINE", "SQUARE", "NOISE", "BELL", "8PULSE", "4PULSE", "ASM_NOISE"]
WAVE_TABLES = []                   # 64 sample cycles from src/audio/internal/data.c (gWaveSamples)


class WaveSample:
    """One cycle of a built-in waveform, looped. Audio_BuildSyntheticWave plays a wave note at
    1.0465 x 32 kHz / 64 samples a cycle at C4, i.e. the waves sound an octave above the note
    (C4 -> 523 Hz)."""
    def __init__(self, cycle):
        self.data = [v / 32768.0 for v in cycle] + [cycle[0] / 32768.0]
        self.loop = (0, len(cycle))
        self.rate = 523.25 * len(cycle)
        self.base = 60


def load_wave_tables(oot):
    src = open(os.path.join(oot, "src", "audio", "internal", "data.c")).read()
    order = re.search(r"gWaveSamples\[\]\s*=\s*\{([^}]*)\}", src).group(1)
    for name in [t.strip() for t in order.split(",") if t.strip()]:
        body = re.search(r"\b%s\[\]\s*=\s*\{([^}]*)\}" % re.escape(name), src).group(1)
        vals = [int(v) for v in re.findall(r"-?\d+", re.sub(r"/\*.*?\*/|//[^\n]*", "", body, flags=re.S))]
        WAVE_TABLES.append(WaveSample(vals[:64]))


def pitch_index(tok):
    """PITCH_A0 = 0, PITCH_BF0 = 1, PITCH_B0 = 2, PITCH_C1 = 3 ... (MIDI note = index + 21)."""
    m = re.match(r"PITCH_([A-G]F?)(\d)$", tok)
    if not m:
        return None
    name, octave = m.group(1), int(m.group(2))
    base = NOTE_NAMES.index(name)
    # A, BF and B carry the octave they sit in; C .. AF are numbered from C, one octave up
    return octave * 12 + base if base <= 2 else (octave - 1) * 12 + base


def note_name_midi(name):
    i = pitch_index("PITCH_" + name)
    return None if i is None else i + 21


# ---------------------------------------------------------------------------------------------
# Samples and soundfonts
# ---------------------------------------------------------------------------------------------
class Sample:
    def __init__(self, path):
        with wave.open(path, "rb") as w:
            self.rate = w.getframerate()
            n = w.getnframes()
            raw = w.readframes(n)
            width = w.getsampwidth()
            ch = w.getnchannels()
        if width != 2:
            raise ValueError(f"{path}: {width * 8} bit")
        vals = struct.unpack("<%dh" % (len(raw) // 2), raw)
        if ch > 1:
            vals = vals[::ch]
        self.data = [v / 32768.0 for v in vals]
        self.base = 60
        self.loop = None
        d = open(path, "rb").read()
        i = 12
        while i + 8 <= len(d):
            cid = d[i:i + 4]
            size = struct.unpack("<I", d[i + 4:i + 8])[0]
            body = d[i + 8:i + 8 + size]
            if cid == b"smpl" and len(body) >= 36:
                self.base = struct.unpack("<I", body[12:16])[0]
                nloops = struct.unpack("<I", body[28:32])[0]
                if nloops and len(body) >= 36 + 24:
                    start, end = struct.unpack("<II", body[36 + 8:36 + 16])
                    if end > start:
                        self.loop = (start, end)
            i += 8 + size + (size & 1)


class Font:
    def __init__(self, xml_path, samplebanks, sample_root):
        root = ET.parse(xml_path).getroot()
        bank = os.path.basename(root.get("SampleBank", "")).replace(".xml", "")
        self.samples = samplebanks.get(bank, {})
        self.sample_root = sample_root
        self.instruments = {}
        self.effects = []
        self.drums = []
        self.envelopes = {}
        for env in root.iter("Envelope"):
            self.envelopes[env.get("Name")] = int(env.get("Release", "0") or 0)
        for e in root.iter("Effect"):
            self.effects.append(self._tuned(e.get("Sample"), e.get("SampleRate"), e.get("BaseNote")))
        for d in root.iter("Drum"):
            self.drums.append(self._tuned(d.get("Sample"), d.get("SampleRate"), d.get("BaseNote")))
        for ins in root.iter("Instrument"):
            num = int(ins.get("ProgramNumber"))
            entry = {
                "mid": self._tuned(ins.get("Sample"), ins.get("SampleRate"), ins.get("BaseNote")),
                "lo": None, "hi": None,
                "range_lo": note_name_midi(ins.get("RangeLo")) if ins.get("RangeLo") else None,
                "range_hi": note_name_midi(ins.get("RangeHi")) if ins.get("RangeHi") else None,
                "release": self.envelopes.get(ins.get("Envelope"), 0),
            }
            if ins.get("SampleLo"):
                entry["lo"] = self._tuned(ins.get("SampleLo"), ins.get("SampleRateLo"), ins.get("BaseNoteLo"))
            if ins.get("SampleHi"):
                entry["hi"] = self._tuned(ins.get("SampleHi"), ins.get("SampleRateHi"), ins.get("BaseNoteHi"))
            self.instruments[num] = entry

    def _tuned(self, name, rate, base):
        if not name or name not in self.samples:
            return None
        return (self.samples[name], int(rate) if rate else None, note_name_midi(base) if base else None)

    def load(self, tuned):
        path, rate, base = tuned
        s = SAMPLE_CACHE.get(path)
        if s is None:
            s = Sample(path)
            SAMPLE_CACHE[path] = s
        return s, rate or s.rate, base if base is not None else s.base

    def voice(self, instr, midi):
        """(Sample, rate, base midi, release) for a note on an instrument."""
        if instr is not None and 128 <= instr < 128 + len(WAVE_TABLES):
            w = WAVE_TABLES[instr - 128]
            return w, w.rate, w.base, 0
        if instr == INSTR_SFX:
            idx = midi - 21
            if 0 <= idx < len(self.effects) and self.effects[idx]:
                s, rate, base = self.load(self.effects[idx])
                return s, rate, midi, 0          # effects play at their own rate
            return None
        if instr == INSTR_DRUM:
            idx = midi - 21
            if 0 <= idx < len(self.drums) and self.drums[idx]:
                s, rate, base = self.load(self.drums[idx])
                return s, rate, midi, 0
            return None
        ins = self.instruments.get(instr)
        if not ins:
            return None
        pick = ins["mid"]
        if ins["range_lo"] is not None and midi <= ins["range_lo"] and ins["lo"]:
            pick = ins["lo"]
        elif ins["range_hi"] is not None and midi > ins["range_hi"] and ins["hi"]:
            pick = ins["hi"]
        if not pick:
            return None
        s, rate, base = self.load(pick)
        return s, rate, base, ins["release"]


SAMPLE_CACHE = {}


def load_fonts(audio_dir):
    samplebanks = {}
    for f in os.listdir(os.path.join(audio_dir, "samplebanks")):
        if not f.endswith(".xml"):
            continue
        root = ET.parse(os.path.join(audio_dir, "samplebanks", f)).getroot()
        names = {}
        for s in root.iter("Sample"):
            p = s.get("Path").replace("$(BUILD_DIR)/assets/audio/", "")
            p = os.path.join(audio_dir, p.replace(".aifc", ".wav"))
            names[s.get("Name")] = p
        samplebanks[f[:-4]] = names
    fonts = {}
    for f in os.listdir(os.path.join(audio_dir, "soundfonts")):
        m = re.match(r"Soundfont_(\d+)\.xml$", f)
        if m:
            fonts[int(m.group(1))] = Font(os.path.join(audio_dir, "soundfonts", f), samplebanks,
                                          os.path.join(audio_dir, "samples"))
    return fonts


# ---------------------------------------------------------------------------------------------
# Sequence parsing
# ---------------------------------------------------------------------------------------------
def split_args(text):
    """Comma separated, but not inside parentheses: FRAMERATE_CONST(24, 28) is one argument."""
    out, depth, cur = [], 0, ""
    for c in text:
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
        if c == "," and depth == 0:
            out.append(cur.strip())
            cur = ""
        else:
            cur += c
    if cur.strip():
        out.append(cur.strip())
    return out


class Program:
    def __init__(self, path):
        self.ops = []          # (mnemonic, [args])
        self.labels = {}       # label -> op index
        self.arrays = {}       # array label -> [tokens]
        cur_array = None
        for line in open(path):
            line = line.split("//")[0].rstrip()
            if not line.strip():
                continue
            m = re.match(r"\s*\.(channel|layer|sequence)\s+(\w+)", line)
            if m:
                self.labels[m.group(2)] = len(self.ops)
                cur_array = None
                continue
            m = re.match(r"\s*\.array\s+(\w+)", line)
            if m:
                cur_array = m.group(1)
                self.arrays[cur_array] = []
                self.labels[cur_array] = len(self.ops)
                continue
            m = re.match(r"\s*\.byte\s+(.*)", line)
            if m and cur_array:
                self.arrays[cur_array] += [t.strip() for t in m.group(1).split(",") if t.strip()]
                continue
            m = re.match(r"^(\w+):\s*$", line)
            if m:
                self.labels[m.group(1)] = len(self.ops)
                cur_array = None
                continue
            m = re.match(r"\s*/\*.*?\*/\s*(\w+)\s*(.*)$", line)
            if m:
                cur_array = None
                self.ops.append((m.group(1), split_args(m.group(2))))


def sym_value(tok, consts):
    """Numbers, PITCH_*, SFn_INST_n / SFn_EFFECT_n / FONTANY_* and IO ports."""
    tok = tok.strip()
    try:
        return int(tok, 0)
    except ValueError:
        pass
    m = re.match(r"FRAMERATE_CONST\(\s*([^,]+),", tok)
    if m:
        return sym_value(m.group(1), consts)   # NTSC value (the second one is PAL)
    p = pitch_index(tok)
    if p is not None:
        return p
    m = re.match(r"SF\d+_(INST|EFFECT|DRUM)_(\d+)$", tok)
    if m:
        return int(m.group(2))
    if tok == "FONTANY_INSTR_SFX":
        return INSTR_SFX
    if tok == "FONTANY_INSTR_DRUM":
        return INSTR_DRUM
    m = re.match(r"FONTANY_INSTR_(\w+)$", tok)
    if m and m.group(1) in FONTANY_WAVES:
        return 128 + FONTANY_WAVES.index(m.group(1))
    m = re.match(r"IO_PORT_(\d+)$", tok)
    if m:
        return int(m.group(1))
    if tok in consts:
        return consts[tok]
    m = re.match(r"Soundfont_(\d+)_ID$", tok)
    if m:
        return int(m.group(1))
    return 0


# ---------------------------------------------------------------------------------------------
# Interpreter
# ---------------------------------------------------------------------------------------------
class Note:
    __slots__ = ("start", "dur", "instr", "font", "midi", "vel", "porta_from", "porta_to", "porta_frac", "legato_with")


class Interpreter:
    MAX_STEPS = 4000

    def __init__(self, prog, max_ticks):
        self.prog = prog
        self.max_ticks = max_ticks
        self.notes = []

    def run_channel(self, label, font=0, instr=None):
        st = {"pc": self.prog.labels[label], "stack": [], "loops": [], "tr": 0, "io": {},
              "time": 0.0, "instr": instr, "transpose": 0, "font": font}
        layers = []
        steps = 0
        while steps < self.MAX_STEPS and st["time"] < self.max_ticks and st["pc"] < len(self.prog.ops):
            steps += 1
            op, args = self.prog.ops[st["pc"]]
            st["pc"] += 1
            v = lambda i: sym_value(args[i], {}) if i < len(args) else 0
            if op == "ldlayer":
                layers.append((st["time"], args[1], st["instr"], st["transpose"], st["font"]))
            elif op in ("delay", "cdelay"):
                st["time"] += v(0)
            elif op == "delay1":
                st["time"] += 1
            elif op == "instr":
                st["instr"] = v(0)
            elif op == "font":
                st["font"] = v(0)
            elif op == "transpose":
                st["transpose"] = v(0)
            elif not self._flow(op, args, st):
                if op == "end":
                    break
        for start, lab, instr, tr, fnt in layers:
            if lab in self.prog.labels:
                self.run_layer(lab, start, instr, tr, fnt)
        return self.notes

    def _flow(self, op, args, st):
        """Control flow and TR arithmetic shared by channels and layers. True if handled."""
        v = lambda i: sym_value(args[i], {}) if i < len(args) else 0
        target = lambda i: self.prog.labels.get(args[i].split("+")[0].strip()) if i < len(args) else None
        if op in ("jump", "rjump"):
            t = target(0)
            if t is None:
                return False
            st["pc"] = t
        elif op == "call":
            t = target(0)
            if t is not None and len(st["stack"]) < 8:
                st["stack"].append(st["pc"])
                st["pc"] = t
        elif op == "end":
            if st["stack"]:
                st["pc"] = st["stack"].pop()
            else:
                return False
        elif op == "loop":
            st["loops"].append([st["pc"], max(1, v(0))])
        elif op == "loopend":
            if st["loops"]:
                st["loops"][-1][1] -= 1
                if st["loops"][-1][1] > 0:
                    st["pc"] = st["loops"][-1][0]
                else:
                    st["loops"].pop()
        elif op == "break":
            if st["loops"]:
                st["loops"].pop()
        elif op == "ldi":
            st["tr"] = v(0)
        elif op == "rand":
            n = v(0)
            st["tr"] = random.randrange(n if n > 0 else 256)
        elif op == "sub":
            st["tr"] -= v(0)
        elif op == "and":
            st["tr"] &= v(0)
        elif op == "ldio":
            st["tr"] = st["io"].get(v(0), 0)
        elif op == "stio":
            st["io"][v(0)] = st["tr"]
        elif op in ("beqz", "rbeqz"):
            if st["tr"] == 0:
                t = target(0)
                if t is not None:
                    st["pc"] = t
        elif op in ("bltz", "rbltz"):
            if st["tr"] < 0:
                t = target(0)
                if t is not None:
                    st["pc"] = t
        elif op == "bgez":
            if st["tr"] >= 0:
                t = target(0)
                if t is not None:
                    st["pc"] = t
        else:
            return False
        return True

    def run_layer(self, label, start, instr, transpose, font):
        st = {"pc": self.prog.labels[label], "stack": [], "loops": [], "tr": 0, "io": {},
              "time": float(start), "instr": instr, "transpose": 0, "font": font}
        porta = None
        legato = False
        last_delay = 0
        prev = None
        steps = 0
        while steps < self.MAX_STEPS and st["time"] < self.max_ticks and st["pc"] < len(self.prog.ops):
            steps += 1
            op, args = self.prog.ops[st["pc"]]
            st["pc"] += 1
            v = lambda i: sym_value(args[i], {}) if i < len(args) else 0
            if op == "instr":
                st["instr"] = v(0)
            elif op == "transpose":
                st["transpose"] = v(0)
            elif op == "ldelay":
                st["time"] += v(0)
                prev = None
            elif op == "legato":
                legato = True
            elif op == "nolegato":
                legato = False
            elif op == "portamento":
                porta = (v(0), v(1), v(2))
            elif op.startswith("note"):
                kind = op[4:]
                pitch = v(0)
                delay, vel, gate = last_delay, 100, 255
                if kind in ("dv", "ldv"):
                    delay, vel = v(1), v(2)
                elif kind == "dvg":
                    delay, vel, gate = v(1), v(2), v(3)
                elif kind == "vg":
                    vel, gate = v(1), v(2)
                elif kind == "pan":
                    continue
                last_delay = delay
                n = Note()
                n.start = st["time"]
                n.dur = (delay * (gate / 255.0 if gate else 1.0)) if delay else None
                n.instr = st["instr"]
                n.font = st["font"]
                offset = transpose + st["transpose"]   # for SFX / drum instruments it moves the index
                n.midi = pitch + 21 + offset
                n.vel = vel
                # Portamento (AudioSeq_SeqLayerProcessScriptStep4): modes 1, 3, 5 slide from the
                # given pitch to the note, 2 and 4 from the note to it. 0x80 ("special") means the
                # time is in 256ths of the note; modes 1 and 2 only last one note.
                n.porta_from = n.porta_to = None
                n.porta_frac = 1.0
                if porta:
                    mode, target = porta[0] & 0x7F, porta[1] + 21 + offset
                    if mode in (2, 4):
                        n.porta_from, n.porta_to = n.midi, target
                    else:
                        n.porta_from, n.porta_to = target, n.midi
                    n.porta_frac = min(1.0, porta[2] / 256.0) if porta[0] & 0x80 else min(1.0, porta[2] / 64.0)
                    if mode in (1, 2):
                        porta = None
                n.legato_with = prev if legato else None
                self.notes.append(n)
                prev = n
                st["time"] += delay
            elif not self._flow(op, args, st):
                if op == "end":
                    break


# ---------------------------------------------------------------------------------------------
# Rendering
# ---------------------------------------------------------------------------------------------
def render(notes, fonts, max_seconds):
    length = int(max_seconds * OUT_RATE) + OUT_RATE
    out = [0.0] * length
    end = 0
    positions = {}
    for n in notes:
        font = fonts.get(n.font) or fonts.get(0)
        if n.instr is None or not font:
            continue
        voice = font.voice(n.instr, n.midi)
        if not voice:
            continue
        sample, rate, base, release = voice
        t0 = int(n.start / TICKS_PER_SEC * OUT_RATE)
        if n.dur is None:
            dur_samples = None
        else:
            dur_samples = int(n.dur / TICKS_PER_SEC * OUT_RATE)
        fade = int(OUT_RATE * min(0.4, max(0.012, (256 - (release or 240)) * 0.004)))
        amp = (n.vel / 127.0) ** 2
        pos = positions.get(id(n.legato_with), 0.0) if n.legato_with else 0.0
        if n.porta_from is not None:
            step_start = 2 ** ((n.porta_from - base) / 12.0) * rate / OUT_RATE
            step_end = 2 ** ((n.porta_to - base) / 12.0) * rate / OUT_RATE
        else:
            step_start = step_end = 2 ** ((n.midi - base) / 12.0) * rate / OUT_RATE
        slide = max(1, int((dur_samples or OUT_RATE) * n.porta_frac))
        data = sample.data
        nd = len(data)
        total = (dur_samples if dur_samples is not None else 10 ** 9) + fade
        i = 0
        while i < total:
            idx = int(pos)
            if idx >= nd - 1:
                if sample.loop and dur_samples is not None:
                    ls, le = sample.loop
                    pos = ls + (pos - le) % max(1, le - ls) if pos >= le else pos
                    idx = int(pos)
                    if idx >= nd - 1:
                        break
                else:
                    break
            if sample.loop and dur_samples is not None and pos >= sample.loop[1]:
                ls, le = sample.loop
                pos = ls + (pos - le) % max(1, le - ls)
                idx = int(pos)
            frac = pos - idx
            s = data[idx] * (1 - frac) + data[idx + 1] * frac
            g = amp
            if dur_samples is not None and i >= dur_samples:
                g *= max(0.0, 1.0 - (i - dur_samples) / fade)
            o = t0 + i
            if o >= length:
                break
            out[o] += s * g
            # Audio_GetPortamentoFreqScale eases along gBendPitchOneOctaveFrequencies (2^x - 1)
            k = 2.0 ** min(1.0, i / slide) - 1.0
            pos += step_start + (step_end - step_start) * k
            i += 1
        positions[id(n)] = pos
        end = max(end, min(length, t0 + i))
    out = out[:end]
    peak = max((abs(x) for x in out), default=0)
    if peak > 0.98:
        out = [x * 0.98 / peak for x in out]
    return out


def write_wav(path, data):
    pcm = struct.pack("<%dh" % len(data), *(max(-32768, min(32767, int(x * 32767))) for x in data))
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(OUT_RATE)
        w.writeframes(pcm)


# ---------------------------------------------------------------------------------------------
def sfx_channels(oot):
    """NA_SE name -> (channel label, bank name)."""
    table = {}
    for f in os.listdir(os.path.join(oot, "include", "tables", "sfx")):
        bank = f.replace("_table.h", "")
        for m in re.finditer(r"DEFINE_SFX\(\s*(\w+),\s*(\w+),", open(os.path.join(oot, "include", "tables", "sfx", f)).read()):
            table[m.group(2)] = (m.group(1), bank)
    return table


def bank_defaults(prog, bank):
    """The instrument and font a bank's handler channel sets before it calls a sound's script."""
    instr, font = None, 0
    label = bank + "_handler_1"
    if label not in prog.labels:
        return instr, font
    pc = prog.labels[label]
    for _ in range(60):
        if pc >= len(prog.ops):
            break
        op, args = prog.ops[pc]
        pc += 1
        if op == "instr" and args:
            instr = sym_value(args[0], {})
        elif op == "font" and args:
            font = sym_value(args[0], {})
        elif op in ("rjump", "jump") and args and args[0] in prog.labels:
            pc = prog.labels[args[0]]
        elif op == "dyncall":
            break
    return instr, font


def voice_takes(prog, chan):
    """Voice clips: the channel points ldseq at an array of SFn_EFFECT_n (one per take) and an
    array of velocities, then picks one at random. Returns [(effect, velocity)] or None."""
    pc = prog.labels[chan]
    arrays = []
    for op, args in prog.ops[pc:pc + 12]:
        # ldptr ARRAY (shared routine reads it) or ldseq ARRAY (read right here)
        if op in ("ldptr", "ldseq") and args and args[0] in prog.arrays:
            arrays.append(prog.arrays[args[0]])
        if op in ("rjump", "jump", "end", "ldlayer"):
            break
    if not arrays or not all(re.match(r"SF\d+_EFFECT_\d+$", t) for t in arrays[0]):
        return None
    vels = arrays[1] if len(arrays) > 1 else ["127"] * len(arrays[0])
    font = int(re.match(r"SF(\d+)_", arrays[0][0]).group(1))
    out = []
    for i, t in enumerate(arrays[0]):
        vel = sym_value(vels[i] if i < len(vels) else "127", {})
        out.append((font, sym_value(t, {}), vel))
    # drop repeats (some voices list the same take twice to weight it)
    seen, uniq = set(), []
    for x in out:
        if x[1] not in seen:
            seen.add(x[1])
            uniq.append(x)
    return uniq


def wanted_sounds():
    src = open(os.path.join(REPO, "pm_shared", "zl_sounds.h")).read()
    return sorted(set(re.findall(r'"(NA_SE_\w+)"', src)))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--oot", required=True)
    ap.add_argument("--version", default="ntsc-1.0")
    ap.add_argument("--out", default=os.path.join(REPO, "zelda", "sound", "zelda", "oot"))
    ap.add_argument("--all", action="store_true", help="every NA_SE_* instead of the ones the game uses")
    ap.add_argument("--max-seconds", type=float, default=2.5, help="cut sounds that loop until stopped")
    ap.add_argument("--only", nargs="*", help="just these NA_SE names")
    args = ap.parse_args()

    audio = os.path.join(args.oot, "extracted", args.version, "assets", "audio")
    seq = os.path.join(args.oot, "assets", "audio", "sequences", "seq_0.prg.seq")
    if not os.path.isdir(os.path.join(audio, "samples")):
        sys.exit(f"{audio}/samples not found. Run 'make setup VERSION={args.version}' in the decomp first.")

    random.seed(64)
    fonts = load_fonts(audio)
    load_wave_tables(args.oot)
    prog = Program(seq)
    chans = sfx_channels(args.oot)
    names = args.only or (sorted(chans) if args.all else wanted_sounds())
    os.makedirs(args.out, exist_ok=True)

    variants = {}
    vpath = os.path.join(args.out, "variants.txt")
    if args.only and os.path.exists(vpath):
        # Just a few sounds: keep what's listed for the others
        for line in open(vpath):
            parts = line.split()
            if len(parts) == 2 and not line.startswith("//") and parts[0] not in names:
                variants[parts[0]] = int(parts[1])
    done = 0
    for name in names:
        chan, bank = chans.get(name, (None, None))
        if not chan or chan not in prog.labels:
            print(f"  {name}: not in the sfx tables")
            continue
        takes = voice_takes(prog, chan)
        if takes:
            for i, (font, eff, vel) in enumerate(takes):
                n = Note()
                n.start, n.dur, n.instr, n.font, n.midi, n.vel = 0, None, INSTR_SFX, font, eff + 21, vel
                n.porta_from = n.porta_to = n.legato_with = None
                n.porta_frac = 1.0
                data = render([n], fonts, args.max_seconds)
                fname = name + (f"_{i + 1}" if i else "") + ".wav"
                write_wav(os.path.join(args.out, fname), data)
            if len(takes) > 1:
                variants[name] = len(takes)
            done += 1
            continue
        interp = Interpreter(prog, args.max_seconds * TICKS_PER_SEC)
        instr, font = bank_defaults(prog, bank)
        notes = interp.run_channel(chan, font, instr)
        data = render(notes, fonts, args.max_seconds)
        if not data:
            print(f"  {name}: no notes")
            continue
        write_wav(os.path.join(args.out, name + ".wav"), data)
        done += 1

    with open(vpath, "w", newline="\n") as f:
        f.write("// Written by tools/zelda/extract_sfx.py: sounds with several takes (NAME.wav, NAME_2.wav ...)\n")
        for k, v in sorted(variants.items()):
            f.write(f"{k} {v}\n")
    print(f"rendered {done}/{len(names)} sounds into {args.out} ({sum(1 for n in names if n in variants)} with several takes)")


if __name__ == "__main__":
    main()
