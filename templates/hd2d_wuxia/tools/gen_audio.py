#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""程序化生成 BGM / 音效（纯 Python，无外部依赖）。

音色思路：Karplus-Strong 拨弦模拟古筝/琵琶，正弦 pad 铺底，
噪声+低频正弦做鼓，简单延时做山谷回声，整体走五声音阶（武侠味）。
"""
import array
import math
import os
import random
import struct
import wave

SR = 22050
random.seed(20260915)


def _pluck(freq, dur, amp=0.5, damping=0.996, bright=0.5):
    n = max(2, int(SR / max(30.0, freq)))
    buf = [random.uniform(-1.0, 1.0) * bright for _ in range(n)]
    total = int(dur * SR)
    out = [0.0] * total
    for i in range(total):
        v = buf[i % n]
        out[i] = v * amp
        nxt = 0.5 * (buf[i % n] + buf[(i + 1) % n]) * damping
        buf[i % n] = nxt
    for i in range(total):
        env = math.exp(-2.6 * i / max(1, total))
        out[i] *= env
    return out


def _sine(freq, dur, amp=0.2, attack=0.05, release=0.4, detune=0.0):
    total = int(dur * SR)
    out = [0.0] * total
    for i in range(total):
        t = i / SR
        env = min(1.0, t / max(1e-4, attack))
        if t > dur - release:
            env *= max(0.0, (dur - t) / max(1e-4, release))
        ph = 2 * math.pi * freq * t
        s = math.sin(ph)
        if detune:
            s += 0.6 * math.sin(ph * (1.0 + detune))
        out[i] = s * amp * env
    return out


def _noise(dur, amp=0.4, decay=18.0, lowpass=0.35):
    total = int(dur * SR)
    out = [0.0] * total
    prev = 0.0
    for i in range(total):
        n = random.uniform(-1.0, 1.0)
        prev = prev + lowpass * (n - prev)
        out[i] = prev * amp * math.exp(-decay * i / max(1, total))
    return out


def _kick(dur=0.32, amp=0.9, f0=120.0, f1=46.0):
    total = int(dur * SR)
    out = [0.0] * total
    ph = 0.0
    for i in range(total):
        t = i / SR
        f = f1 + (f0 - f1) * math.exp(-26 * t)
        ph += 2 * math.pi * f / SR
        out[i] = math.sin(ph) * amp * math.exp(-7.5 * t)
    return out


def _mix_into(dst, src, at, gain=1.0):
    end = min(len(dst), at + len(src))
    for i in range(max(0, end - at)):
        dst[at + i] += src[i] * gain


def _echo(buf, delay=0.31, feed=0.28, times=3):
    out = list(buf)
    for k in range(1, times + 1):
        off = int(delay * SR * k)
        g = feed ** k
        for i in range(len(buf) - off):
            out[off + i] += buf[i] * g
    return out


def _normalize(buf, target=0.82):
    peak = max(1e-6, max(abs(v) for v in buf))
    return [max(-1.0, min(1.0, v / peak * target)) for v in buf]


def _write(path, buf, fade=0.05):
    n = len(buf)
    f = int(fade * SR)
    for i in range(min(f, n)):
        g = i / max(1, f)
        buf[i] *= g
        buf[n - 1 - i] *= g
    data = array.array("h", (int(max(-1.0, min(1.0, v)) * 32000) for v in buf))
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(data.tobytes())


NOTE = {}
_A4 = 440.0
for i, name in enumerate(["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]):
    for octv in range(1, 7):
        NOTE["%s%d" % (name, octv)] = _A4 * (2 ** ((i - 9) / 12.0 + (octv - 4)))


def _seq(buf, notes, gain=0.5, dur=0.9, damping=0.996):
    for (t, name, ln, amp) in notes:
        if name not in NOTE:
            continue
        _mix_into(buf, _pluck(NOTE[name], ln * dur, amp * gain, damping), int(t * SR))


def _pad(buf, chords, gain=0.16):
    for (t, names, ln) in chords:
        for nm in names:
            _mix_into(buf, _sine(NOTE[nm], ln, gain, 0.6, ln * 0.7, detune=0.004), int(t * SR))


def make_bgm(kind):
    if kind == "title":
        dur = 20.0
        buf = [0.0] * int(dur * SR)
        _pad(buf, [(0, ["A2", "E3", "A3"], 8), (8, ["F2", "C3", "F3"], 6), (14, ["G2", "D3", "G3"], 6)], 0.13)
        mel = [(0.4, "A4", 1.0, 0.55), (1.2, "C5", 0.8, 0.42), (2.0, "E5", 1.4, 0.5),
               (3.6, "D5", 0.9, 0.4), (4.5, "C5", 1.6, 0.45), (6.4, "A4", 1.2, 0.4),
               (8.4, "F4", 1.0, 0.5), (9.3, "A4", 0.8, 0.4), (10.1, "C5", 1.6, 0.46),
               (12.0, "D5", 1.0, 0.42), (13.0, "C5", 1.0, 0.4), (14.0, "A4", 2.0, 0.5),
               (16.4, "G4", 1.0, 0.38), (17.3, "A4", 2.2, 0.44)]
        _seq(buf, mel, 0.6)
        return _normalize(_echo(buf, 0.34, 0.3, 3))
    if kind == "field":
        dur = 24.0
        buf = [0.0] * int(dur * SR)
        _pad(buf, [(0, ["D3", "A3", "D4"], 8), (8, ["B2", "F3", "B3"], 8), (16, ["G2", "D3", "G3"], 8)], 0.12)
        mel = []
        base = ["D5", "F5", "G5", "A5", "C6", "A5", "G5", "F5"]
        for k in range(22):
            t = 0.5 + k * 1.05
            mel.append((t, base[k % len(base)], 0.8, 0.30 + (k % 3) * 0.05))
        _seq(buf, mel, 0.5)
        for k in range(6):
            _mix_into(buf, _kick(0.3, 0.35), int((2.0 + k * 4.0) * SR))
        return _normalize(_echo(buf, 0.26, 0.22, 2), 0.78)
    if kind == "battle":
        dur = 20.0
        buf = [0.0] * int(dur * SR)
        _pad(buf, [(0, ["E3", "B3", "E4"], 10), (10, ["C3", "G3", "C4"], 10)], 0.12)
        ost = []
        for k in range(40):
            ost.append((k * 0.5, ["E5", "G5", "B4", "E5"][k % 4], 0.5, 0.3))
        _seq(buf, ost, 0.42, 0.6)
        for k in range(20):
            t = k * 1.0
            _mix_into(buf, _kick(0.28, 0.85), int(t * SR))
            if k % 2 == 1:
                _mix_into(buf, _noise(0.16, 0.45, 26), int((t + 0.5) * SR))
        mel = [(1.0, "E5", 1.0, 0.4), (2.0, "G5", 0.8, 0.36), (3.0, "A5", 1.2, 0.4),
               (5.0, "B5", 1.0, 0.38), (6.0, "A5", 1.6, 0.4), (11.0, "G5", 1.0, 0.36),
               (12.0, "E5", 1.2, 0.4), (14.0, "D5", 1.0, 0.34), (15.0, "E5", 2.0, 0.42)]
        _seq(buf, mel, 0.5)
        return _normalize(buf, 0.86)
    # boss
    dur = 22.0
    buf = [0.0] * int(dur * SR)
    for i in range(int(dur * SR)):
        t = i / SR
        buf[i] += 0.10 * math.sin(2 * math.pi * NOTE["D2"] * t) * (1.0 - 0.4 * (t / dur))
        buf[i] += 0.05 * math.sin(2 * math.pi * NOTE["A2"] * t * 1.005)
    _pad(buf, [(0, ["D3", "F3", "A3"], 11), (11, ["C3", "E3", "G3"], 11)], 0.13)
    for k in range(22):
        _mix_into(buf, _kick(0.34, 0.95, 150, 40), int(k * 1.0 * SR))
        if k % 4 == 2:
            _mix_into(buf, _noise(0.5, 0.5, 9), int((k + 0.5) * SR))
    _seq(buf, [(0.5, "D5", 0.7, 0.34), (1.3, "F5", 0.7, 0.32), (2.1, "A5", 1.0, 0.38),
               (3.4, "G5", 0.8, 0.32), (4.4, "F5", 1.4, 0.36), (7.0, "D5", 0.8, 0.34),
               (8.0, "C5", 1.0, 0.32), (9.4, "D5", 1.8, 0.38)], 0.5)
    return _normalize(_echo(buf, 0.42, 0.3, 2), 0.88)


def make_sfx(kind):
    if kind == "slash":
        buf = _noise(0.22, 0.9, 14, 0.55)
        for i in range(len(buf)):
            buf[i] *= 0.5 + 0.5 * math.sin(2 * math.pi * 900 * i / SR)
        return _normalize(buf, 0.7)
    if kind == "hit":
        buf = _kick(0.28, 0.9, 260, 70)
        _mix_into(buf, _noise(0.14, 0.6, 24, 0.6), 0)
        return _normalize(buf, 0.8)
    if kind == "crit":
        buf = _kick(0.4, 1.0, 320, 60)
        _mix_into(buf, _noise(0.2, 0.7, 18, 0.5), 0)
        _mix_into(buf, _sine(NOTE["A5"], 0.35, 0.35, 0.005, 0.3, 0.01), 0)
        return _normalize(buf, 0.9)
    if kind == "die":
        buf = [0.0] * int(0.9 * SR)
        _mix_into(buf, _sine(NOTE["A4"], 0.8, 0.4, 0.01, 0.6), 0)
        _mix_into(buf, _sine(NOTE["D4"], 0.9, 0.35, 0.01, 0.6, 0.01), 0)
        _mix_into(buf, _noise(0.5, 0.3, 8, 0.3), 0)
        return _normalize(buf, 0.7)
    if kind == "dodge":
        buf = _noise(0.3, 0.5, 9, 0.2)
        for i in range(len(buf)):
            buf[i] *= 0.3 + 0.7 * abs(math.sin(2 * math.pi * (300 + 900 * i / len(buf)) * i / SR))
        return _normalize(buf, 0.55)
    if kind == "skill":
        buf = [0.0] * int(1.2 * SR)
        for k, nm in enumerate(["D5", "A5", "D6", "F6"]):
            _mix_into(buf, _pluck(NOTE[nm], 0.9, 0.4), int(k * 0.09 * SR))
        _mix_into(buf, _sine(NOTE["D4"], 1.1, 0.25, 0.02, 0.5, 0.01), 0)
        return _normalize(_echo(buf, 0.18, 0.3, 2), 0.8)
    if kind == "heal":
        buf = [0.0] * int(1.0 * SR)
        for k, nm in enumerate(["G5", "B5", "D6"]):
            _mix_into(buf, _sine(NOTE[nm], 0.8, 0.28, 0.05, 0.5), int(k * 0.08 * SR))
        return _normalize(buf, 0.6)
    if kind == "pickup":
        buf = [0.0] * int(0.4 * SR)
        _mix_into(buf, _pluck(NOTE["E6"], 0.3, 0.5), 0)
        _mix_into(buf, _pluck(NOTE["A6"], 0.3, 0.45), int(0.08 * SR))
        return _normalize(buf, 0.7)
    if kind == "coin_use":
        buf = [0.0] * int(0.5 * SR)
        _mix_into(buf, _sine(NOTE["F6"], 0.3, 0.4, 0.005, 0.25), 0)
        _mix_into(buf, _sine(NOTE["A6"], 0.35, 0.35, 0.005, 0.25), int(0.1 * SR))
        return _normalize(buf, 0.65)
    if kind == "levelup":
        buf = [0.0] * int(1.6 * SR)
        for k, nm in enumerate(["D5", "F5", "A5", "D6", "F6", "A6"]):
            _mix_into(buf, _pluck(NOTE[nm], 1.0, 0.4), int(k * 0.1 * SR))
        _mix_into(buf, _sine(NOTE["D4"], 1.5, 0.3, 0.05, 0.7, 0.01), 0)
        return _normalize(_echo(buf, 0.2, 0.35, 3), 0.85)
    if kind == "talk":
        buf = [0.0] * int(0.09 * SR)
        _mix_into(buf, _sine(880, 0.07, 0.35, 0.004, 0.04), 0)
        return _normalize(buf, 0.45)
    if kind == "ui":
        buf = [0.0] * int(0.16 * SR)
        _mix_into(buf, _sine(1320, 0.12, 0.35, 0.003, 0.09), 0)
        return _normalize(buf, 0.5)
    if kind == "gate":
        buf = [0.0] * int(1.1 * SR)
        _mix_into(buf, _noise(0.9, 0.5, 6, 0.08), 0)
        _mix_into(buf, _sine(120, 1.0, 0.3, 0.05, 0.6), 0)
        return _normalize(buf, 0.6)
    return _normalize(_noise(0.2, 0.5, 20), 0.5)


def generate_audio(out_root):
    audio_dir = os.path.join(out_root, "assets", "audio")
    os.makedirs(audio_dir, exist_ok=True)
    for kind in ("title", "field", "battle", "boss"):
        _write(os.path.join(audio_dir, "bgm_%s.wav" % kind), make_bgm(kind), 0.35)
        print("[audio] bgm_%s.wav" % kind)
    for kind in ("slash", "hit", "crit", "die", "dodge", "skill", "heal", "pickup",
                 "coin_use", "levelup", "talk", "ui", "gate"):
        _write(os.path.join(audio_dir, "sfx_%s.wav" % kind), make_sfx(kind), 0.01)
        print("[audio] sfx_%s.wav" % kind)