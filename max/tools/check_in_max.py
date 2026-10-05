"""Check the Max object and the Max for Live device inside Max against the offline processor.

    python max/tools/check_in_max.py build      writes build/maxcheck/: a test signal and a self-running patch
    open -a Max build/maxcheck/check_<time>.maxpat
                                                plays the signal (by sample index, output muted) through polypitch~ at
                                                several settings and through the device hosted by amxd~, records each,
                                                writes the files and closes its window. About 25 seconds.
    python max/tools/check_in_max.py compare    Max's recordings against the offline processor: should null deeply

The patch turns Max's audio on for its own window only and does not touch Max's audio settings. Do not run it while
another program is using the audio interface at a different sample rate: starting Max's audio can switch the
interface to the rate in Max's settings.
The PolyPitch package (max/PolyPitch) has to be where Max finds it: scripts/install-macos.sh max
"""
import json, os, sys, time, subprocess, tempfile
import numpy as np, soundfile as sf
from maxpatch import obj, msg, line, gen, APPV, ROOT, DEVICE

OUT = os.path.join(ROOT, "build", "maxcheck"); SR = 44100
# (name, object text or None for the device, message sent before audio starts, offline settings)
CASES = [("object_down", "polypitch~ -12 @mix 100 @tone 0", "", dict(st=-12, mix=100, tone=0)), ("object_up", "polypitch~ 12 @mix 100 @tone 0", "", dict(st=12, mix=100, tone=0)),
         ("object_mix50", "polypitch~ -12 @mix 50 @tone 100", "", dict(st=-12, mix=50, tone=100)), ("object_clean", "polypitch~ 12 @mix 100 @tone 0 @response 2", "", dict(st=12, mix=100, tone=0, response=2)),
         ("device", None, "", dict(st=-12, mix=100, tone=100)), ("device_up", None, "Semitones 12", dict(st=12, mix=100, tone=100)),
         ("device_balanced", None, "Semitones 12, Response 1", dict(st=12, mix=100, tone=100, response=1))]


def signal():
    sys.path.insert(0, os.path.join(ROOT, "tests"))
    from signals import NOTE, N, pluck, strum
    z = np.zeros(SR // 2); parts = [z, pluck(NOTE["A2"], 1.5)[0], z, strum(N("E2", "B2", "E3", "G#3", "B3", "E4"), 2.5, 0.5)[0], z, strum(N("A4", "C#5", "E5"), 2.0, 0.5)[0], z]
    return np.concatenate(parts).astype(np.float32)


def patch(sig_wav, nsamp, tag):
    player = 'Buffer sig("sig");\nHistory c(0);\nout1 = peek(sig, c, 0);\nout2 = c;\nc = c + 1;\n'
    srcode = 'Buffer b("pp_sr");\npoke(b, samplerate, 0, 0);\npoke(b, vectorsize, 1, 0);\nout1 = 0;\n'
    dur_ms = int(nsamp / SR * 1000 * 1.15) + 3000
    B = [obj("lb", "loadbang", [40, 20, 60, 22], 1, 1, ["bang"]), obj("t", "t b b b b", [40, 60, 80, 22], 1, 4, ["bang"] * 4),
         obj("bsig", "buffer~ sig", [400, 140, 80, 22], 1, 2, ["float", "bang"]), msg("m_read", f"replace {sig_wav}", [400, 100, 400, 22]),
         obj("bsr", "buffer~ pp_sr 10", [1750, 140, 100, 22], 1, 2, ["float", "bang"]), gen("gsr", [1750, 260, 60, 22], srcode, 1, 1),
         msg("m_wsr", f"format float32, writewave {os.path.join(OUT, tag + '_samplerate.wav')}", [1750, 100, 400, 22]),
         obj("d0", "delay 1500", [200, 60, 70, 22], 2, 1, ["bang"]), obj("d1", "delay 4000", [40, 110, 70, 22], 2, 1, ["bang"]), msg("m_on", "startwindow", [40, 150, 80, 22]),
         obj("d2", f"delay {dur_ms}", [130, 150, 90, 22], 2, 1, ["bang"]), msg("m_off", "stop", [130, 190, 40, 22]),
         obj("d3", "delay 6000", [130, 230, 70, 22], 2, 1, ["bang"]), msg("m_close", "clean, wclose", [130, 270, 80, 22]), obj("tp", "thispatcher", [130, 310, 70, 22], 1, 2, ["", ""]),
         gen("player", [200, 260, 120, 22], player, 0, 2), obj("dac", "dac~", [40, 640, 40, 22], 2, 0), obj("mute", "*~ 0.", [40, 600, 40, 22], 2, 1, ["signal"])]
    Ln = [line("lb", 0, "t", 0), line("t", 3, "m_read", 0), line("m_read", 0, "bsig", 0), line("t", 0, "d1", 0), line("t", 2, "d0", 0),
          line("d1", 0, "m_on", 0), line("d1", 0, "d2", 0), line("m_on", 0, "dac", 0), line("d2", 0, "m_off", 0), line("m_off", 0, "dac", 0), line("d2", 0, "d3", 0),
          line("d3", 0, "m_close", 0), line("m_close", 0, "tp", 0), line("d2", 0, "m_wsr", 0), line("m_wsr", 0, "bsr", 0), line("gsr", 0, "mute", 0), line("mute", 0, "dac", 0)]
    rec = lambda nm: f'Buffer res("res_{nm}");\npoke(res, in1, in2, 0);\nout1 = 0;\n'
    for i, (nm, text, m, _) in enumerate(CASES):
        x = 200 + i * 215                                # one column per case
        B.append(obj(f"x_{nm}", text, [x, 330, 200, 22], 2, 2, ["signal", "signal"]) if text else obj(f"x_{nm}", f'amxd~ "{DEVICE}"', [x, 330, 200, 22], 3, 4, ["signal", "signal", "", ""]))
        if m: B.append(msg(f"m_{nm}", m, [x, 300, 200, 22])); Ln += [line("d0", 0, f"m_{nm}", 0), line(f"m_{nm}", 0, f"x_{nm}", 0)]
        B += [gen(f"r_{nm}", [x, 420, 120, 22], rec(nm), 2, 1), obj(f"b_{nm}", f"buffer~ res_{nm}", [x, 490, 150, 22], 1, 2, ["float", "bang"]), msg(f"s_{nm}", f"sizeinsamps {nsamp}", [x, 460, 150, 22]),
              msg(f"w_{nm}", f"format float32, writewave {os.path.join(OUT, tag + '_' + nm + '.wav')}", [x, 530, 200, 22])]
        Ln += [line("player", 0, f"x_{nm}", 0), line("player", 0, f"x_{nm}", 1), line(f"x_{nm}", 0, f"r_{nm}", 0), line("player", 1, f"r_{nm}", 1), line(f"r_{nm}", 0, "mute", 0),
               line("t", 3, f"s_{nm}", 0), line(f"s_{nm}", 0, f"b_{nm}", 0), line("d2", 0, f"w_{nm}", 0), line(f"w_{nm}", 0, f"b_{nm}", 0)]
    return {"patcher": {"fileversion": 1, "appversion": APPV, "classnamespace": "box", "rect": [60, 60, 1900, 720], "boxes": B, "lines": Ln}}


def offline(x, sr, st, mix, tone, response=0):
    cli = os.path.join(ROOT, "build", "tools", "polypitch_cli")
    with tempfile.TemporaryDirectory() as d:
        a = os.path.join(d, "in.f32"); b = os.path.join(d, "out.f32"); x.astype(np.float32).tofile(a)
        subprocess.run([cli, a, b, str(sr), str(st), str(mix), str(tone), f"response={response}"], check=True, capture_output=True); return np.fromfile(b, np.float32).astype(np.float64)


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True); what = sys.argv[1] if len(sys.argv) > 1 else "build"
    if what == "build":
        x = signal(); wav = os.path.join(OUT, "signal.wav"); sf.write(wav, x, SR, subtype="FLOAT")
        name = os.path.join(OUT, "check_" + time.strftime("%H%M%S") + ".maxpat")      # a fresh name each run: re-opening a file Max still has open does nothing
        json.dump(patch(wav, len(x), "check"), open(name, "w"), indent="\t"); print("now run:  open -a Max", name)
    else:
        x = sf.read(os.path.join(OUT, "signal.wav"), dtype="float32")[0]; s = np.ravel(sf.read(os.path.join(OUT, "check_samplerate.wav"))[0]); sr = int(round(float(s[0])))
        print(f"Max ran at {sr} Hz, vector size {int(s[1])}"); db = lambda a: 20 * np.log10(max(float(np.sqrt(np.mean(np.square(a)))), 1e-15)); ok = True
        for nm, text, m, kw in CASES:
            p = os.path.join(OUT, f"check_{nm}.wav")
            if not os.path.exists(p): print(f"{nm:14s}: no recording"); ok = False; continue
            y = sf.read(p, dtype="float64")[0][:len(x)]; ref = offline(x, sr, **kw); n = min(len(y), len(ref)); r = db(y[:n] - ref[:n]) - db(ref[:n])
            print(f"{nm:14s} ({text or 'the device in amxd~'}{', ' + m if m else ''}): Max minus offline {r:7.1f} dB, peak {20 * np.log10(np.abs(y).max() + 1e-12):6.1f} dBFS"); ok = ok and r < -100
        print("ALL MATCH" if ok else "MISMATCH"); sys.exit(0 if ok else 1)
