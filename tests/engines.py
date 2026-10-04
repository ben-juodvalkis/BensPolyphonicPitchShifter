"""The three things a test can run a signal through. All take a mono float array at `sr` and return the shifted
signal on the same timeline (no dry signal, no tone curve).

    engine     the C++ engine, through build/tools/polypitch_cli (scripts/build.sh tools)
    reference  the Python reference implementation (reference/polypitch_ref.py)
    plugin     the built VST3, hosted headless with pedalboard (scripts/build.sh plugin)
"""
import os, subprocess, sys, tempfile, glob
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
CLI = os.path.join(ROOT, "build", "tools", "polypitch_cli")


def engine(x, st, sr=44100, mix=None, tone=None):
    if not os.path.exists(CLI): raise SystemExit("build the tools first: scripts/build.sh tools")
    with tempfile.TemporaryDirectory() as d:
        a = os.path.join(d, "in.f32"); b = os.path.join(d, "out.f32"); np.asarray(x, np.float32).tofile(a)
        args = [CLI, a, b, str(sr), str(st)] + ([str(mix), str(tone)] if mix is not None else [])
        r = subprocess.run(args, capture_output=True, text=True)
        if r.returncode != 0: raise RuntimeError(r.stderr)
        engine.last_message = r.stderr.strip()
        return np.fromfile(b, np.float32).astype(np.float64)


def reference(x, st, sr=44100, **kw):
    sys.path.insert(0, os.path.join(ROOT, "reference"))
    from polypitch_ref import shift
    return shift(np.asarray(x, np.float64), st, sr=sr, **kw)


def plugin(x, st, sr=44100, mix=100.0, tone=0.0, buffer_size=512):
    from pedalboard import load_plugin
    c = glob.glob(os.path.join(ROOT, "build", "plugin", "**", "PolyPitch.vst3"), recursive=True)
    if not c: raise SystemExit("build the plug-in first: scripts/build.sh plugin")
    p = load_plugin(c[0]); p.semitones = int(st); p.mix = float(mix); p.tone = float(tone)
    xs = np.stack([x, x]).astype(np.float32)
    return p(xs, sr, buffer_size=buffer_size, reset=True)[0].astype(np.float64)


BY_NAME = dict(engine=engine, reference=reference, plugin=plugin)
