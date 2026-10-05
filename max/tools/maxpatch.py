"""Generates the Max files that are not written by hand in Max:

    max/device/Ben's Polyphonic Pitch Shifter.amxd   the Max for Live audio effect (plugin~ -> polypitch~ -> plugout~, three dials, the Response and Quality tabs)
    max/PolyPitch/help/polypitch~.maxhelp    the object's help patch
    build/maxcheck/check_<time>.maxpat       a self-running check patch (see check_in_max.py)

    python max/tools/maxpatch.py             writes the device and the help patch

An .amxd is a small container around a patcher in JSON: "ampf", a version, the device type ("aaaa" = audio effect),
a "meta" chunk and a "ptch" chunk holding the JSON. The device made here is not frozen: it finds polypitch~ through
the PolyPitch package (max/PolyPitch, installed in Max's Packages folder). Freezing, which packs the object into the
device file, is a button in Max's device editor (docs/max.md).
"""
import json, os, struct, time

HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(os.path.dirname(HERE))
NAME = "Ben's Polyphonic Pitch Shifter"      # what Live shows: the device's file name and its title
DEVICE = os.path.join(ROOT, "max", "device", NAME + ".amxd")
APPV = {"major": 9, "minor": 2, "revision": 0, "architecture": "x64", "modernui": 1}


def box(id, maxclass, rect, nin, nout, outtype=None, **kw):
    b = {"id": id, "maxclass": maxclass, "numinlets": nin, "numoutlets": nout, "patching_rect": rect}
    if nout: b["outlettype"] = outtype or [""] * nout
    b.update(kw); return {"box": b}


def obj(id, text, rect, nin, nout, outtype=None, **kw): return box(id, "newobj", rect, nin, nout, outtype, text=text, **kw)
def msg(id, text, rect): return box(id, "message", rect, 2, 1, [""], text=text)
def comment(id, text, rect, **kw): return box(id, "comment", rect, 1, 0, text=text, **kw)
def line(a, ao, b, bi): return {"patchline": {"source": [a, ao], "destination": [b, bi]}}


def gen(id, rect, code, nin, nout):
    bx = [obj(f"in{i + 1}", f"in {i + 1}", [30 + i * 150, 20, 40, 22], 0, 1) for i in range(nin)]
    bx += [obj(f"out{i + 1}", f"out {i + 1}", [30 + i * 150, 700, 45, 22], 1, 0) for i in range(nout)]
    bx.append(box("code", "codebox", [30, 60, 700, 620], max(nin, 1), max(nout, 1), code=code, fontface=0, fontname="<Monospaced>", fontsize=12.0))
    ln = [line(f"in{i + 1}", 0, "code", i) for i in range(nin)] + [line("code", i, f"out{i + 1}", 0) for i in range(nout)]
    p = {"fileversion": 1, "appversion": APPV, "classnamespace": "dsp.gen", "rect": [100, 100, 800, 800], "boxes": bx, "lines": ln}
    return obj(id, "gen~", rect, nin, nout, ["signal"] * nout, patcher=p)


def dial(id, name, order, rect, prect, lo, hi, init, unit, integer=False):
    v = {"parameter_longname": name, "parameter_shortname": name, "parameter_order": order, "parameter_type": 1 if integer else 0, "parameter_modmode": 0 if integer else 3,
         "parameter_mmin": lo, "parameter_mmax": hi, "parameter_initial_enable": 1, "parameter_initial": [init], "parameter_unitstyle": unit}
    if not integer: v["parameter_exponent"] = 1.0
    return box(id, "live.dial", rect, 1, 2, ["", "float"], parameter_enable=1, presentation=1, presentation_rect=prect, varname=name, saved_attribute_attributes={"valueof": v})


def tab(id, name, order, rect, prect, items, init=0):
    # a row of buttons, one of which is on (as Max saves a live.tab: an "enum" parameter, the left outlet gives the index)
    v = {"parameter_longname": name, "parameter_shortname": name, "parameter_order": order, "parameter_type": 2, "parameter_modmode": 0, "parameter_enum": list(items),
         "parameter_mmax": len(items) - 1, "parameter_initial_enable": 1, "parameter_initial": [init], "parameter_unitstyle": 9}
    return box(id, "live.tab", rect, 1, 3, ["", "", "float"], parameter_enable=1, presentation=1, presentation_rect=prect, varname=name, num_lines_patching=1, num_lines_presentation=1,
               saved_attribute_attributes={"valueof": v})


def device_patcher():
    B = [obj("obj-1", "plugin~", [48.0, 34.0, 53.0, 20.0], 2, 2, ["signal", "signal"], fontname="Arial Bold", fontsize=10.0),
         obj("obj-2", "plugout~", [48.0, 374.0, 53.0, 20.0], 2, 2, ["signal", "signal"], fontname="Arial Bold", fontsize=10.0),
         comment("obj-9", "Device vertical limit", [0.0, 430.0, 134.0, 20.0], fontname="Ableton Sans Medium Regular", fontsize=11.0, hidden=1),
         obj("pp", "polypitch~", [48.0, 250.0, 200.0, 22.0], 2, 2, ["signal", "signal"]),
         dial("d_semi", "Semitones", 0, [300, 60, 50, 48], [10, 34, 56, 48], -12, 12, -12, 7, integer=True),
         dial("d_mix", "Mix", 1, [380, 60, 44, 48], [72, 34, 44, 48], 0.0, 100.0, 100.0, 5),
         dial("d_tone", "Tone", 2, [460, 60, 44, 48], [122, 34, 44, 48], 0.0, 100.0, 100.0, 5),
         obj("p_semi", "prepend semitones", [300, 150, 105, 22], 1, 1, [""]), obj("p_mix", "prepend mix", [420, 150, 75, 22], 1, 1, [""]), obj("p_tone", "prepend tone", [510, 150, 80, 22], 1, 1, [""]),
         tab("t_resp", "Response", 3, [540, 60, 156, 18], [10, 106, 156, 18], ("Fast", "Balanced", "Clean")), obj("p_resp", "prepend response", [600, 150, 100, 22], 1, 1, [""]),
         comment("c_resp", "Response (shifting up)", [540, 40, 150, 18], presentation=1, presentation_rect=[8, 90, 160, 16], fontface=0, fontsize=9.0),
         tab("t_qual", "Quality", 4, [540, 220, 156, 18], [10, 144, 156, 18], ("Full", "Lite", "Eco")), obj("p_qual", "prepend quality", [600, 260, 100, 22], 1, 1, [""]),
         comment("c_qual", "Quality (Lite and Eco: less CPU)", [540, 200, 150, 18], presentation=1, presentation_rect=[8, 128, 160, 16], fontface=0, fontsize=9.0),
         comment("c_title", NAME, [300, 20, 200, 20], presentation=1, presentation_rect=[10, 8, 160, 20], fontface=1, fontsize=10.0)]   # 145 px wide in Arial Bold 10
    L = [line("obj-1", 0, "pp", 0), line("obj-1", 1, "pp", 1), line("pp", 0, "obj-2", 0), line("pp", 1, "obj-2", 1),
         line("d_semi", 0, "p_semi", 0), line("p_semi", 0, "pp", 0), line("d_mix", 0, "p_mix", 0), line("p_mix", 0, "pp", 0), line("d_tone", 0, "p_tone", 0), line("p_tone", 0, "pp", 0),
         line("t_resp", 0, "p_resp", 0), line("p_resp", 0, "pp", 0), line("t_qual", 0, "p_qual", 0), line("p_qual", 0, "pp", 0)]
    now = int(time.time()) + 2082844800           # Max counts seconds from 1904
    project = {"version": 1, "creationdate": now, "modificationdate": now, "viewrect": [0.0, 0.0, 300.0, 500.0], "autoorganize": 1, "hideprojectwindow": 1, "showdependencies": 1,
               "autolocalize": 0, "contents": {"patchers": {}}, "layout": {}, "searchpath": {}, "detailsvisible": 0, "amxdtype": 1633771873, "readonly": 0, "devpathtype": 0,
               "devpath": ".", "sortmode": 0, "viewmode": 0, "includepackages": 0}
    return {"patcher": {"fileversion": 1, "appversion": APPV, "classnamespace": "box", "rect": [100.0, 100.0, 760.0, 560.0], "openrect": [0.0, 0.0, 0.0, 169.0], "openrectmode": 0,
                        "default_fontsize": 10.0, "default_fontname": "Arial Bold", "gridsize": [8.0, 8.0], "boxanimatetime": 500, "title": NAME, "boxes": B, "lines": L,
                        "latency": 0, "is_mpe": 0, "external_mpe_tuning_enabled": 0, "minimum_live_version": "", "minimum_max_version": "", "platform_compatibility": 0,
                        "project": project, "autosave": 0, "openinpresentation": 1, "devicewidth": 176.0, "description": "Polyphonic pitch shifter"}}


def write_amxd(path, patcher):
    body = json.dumps(patcher, indent="\t").encode() + b"\n\x00"
    head = b"ampf" + struct.pack("<I", 4) + b"aaaa" + b"meta" + struct.pack("<I", 4) + struct.pack("<I", 0) + b"ptch"
    os.makedirs(os.path.dirname(path), exist_ok=True); open(path, "wb").write(head + struct.pack("<I", len(body)) + body); return path


def help_patcher():
    B = [comment("t", "polypitch~", [20, 15, 300, 30], fontsize=20.0, fontface=1),
         comment("d", "Polyphonic pitch shifter for live playing: chords stay in tune and clean, attacks come out about 2 ms late shifting down and 7 ms shifting up. The shifted sound is mono (the inputs summed); the dry signal passes in stereo and is never delayed.", [20, 50, 520, 48]),
         obj("adc", "adc~ 1 2", [20, 130, 60, 22], 1, 2, ["signal", "signal"]), comment("c_in", "your instrument (or any signal)", [90, 130, 200, 20]),
         obj("pp", "polypitch~ -12", [20, 350, 150, 22], 2, 2, ["signal", "signal"]),
         box("a_semi", "attrui", [200, 170, 200, 22], 1, 1, [""], attr="semitones"), box("a_mix", "attrui", [200, 196, 200, 22], 1, 1, [""], attr="mix"), box("a_tone", "attrui", [200, 222, 200, 22], 1, 1, [""], attr="tone"),
         comment("c_semi", "-12 .. 12 semitones (also the first argument)", [410, 170, 280, 20]), comment("c_mix", "0 = dry only, 50 = both at full level, 100 = shifted only", [410, 196, 330, 20]),
         comment("c_tone", "how much of the tone curve for this interval is applied", [410, 222, 330, 20]),
         box("a_resp", "attrui", [200, 248, 200, 22], 1, 1, [""], attr="response"), comment("c_resp", "shifting up: 0 = fast, 1 = balanced (attacks 4 ms later, cleaner), 2 = clean (8 ms later; a full chord's middle note comes out right)", [410, 244, 330, 34]),
         box("a_qual", "attrui", [200, 282, 200, 22], 1, 1, [""], attr="quality"), comment("c_qual", "0 = full, 1 = lite (about four fifths of the CPU shifting down and under three fifths shifting up), 2 = eco (about a third of full; shifting up nothing above 10.5 kHz)", [410, 278, 330, 34]),
         msg("m_clear", "clear", [200, 316, 45, 22]), comment("c_clear", "forget everything heard so far", [250, 316, 200, 20]),
         box("g", "gain~", [20, 385, 22, 100], 1, 2, ["signal", ""], multichannelvariant=0, parameter_enable=0), box("g2", "gain~", [150, 385, 22, 100], 1, 2, ["signal", ""], multichannelvariant=0, parameter_enable=0),
         box("dac", "ezdac~", [20, 500, 45, 45], 2, 0), comment("c_dac", "start audio", [75, 512, 100, 20]),
         comment("c_more", "Latency: none is reported and the dry signal is not delayed. Sample rates: 44.1 and 48 kHz use the designed band filters; other rates fall back to a simpler, leakier filter.", [20, 560, 620, 34])]
    L = [line("adc", 0, "pp", 0), line("adc", 1, "pp", 1), line("pp", 0, "g", 0), line("pp", 1, "g2", 0), line("g", 0, "dac", 0), line("g2", 0, "dac", 1), line("g", 1, "g2", 0),
         line("a_semi", 0, "pp", 0), line("a_mix", 0, "pp", 0), line("a_tone", 0, "pp", 0), line("a_resp", 0, "pp", 0), line("a_qual", 0, "pp", 0), line("m_clear", 0, "pp", 0)]
    return {"patcher": {"fileversion": 1, "appversion": APPV, "classnamespace": "box", "rect": [100, 100, 780, 640], "boxes": B, "lines": L}}


if __name__ == "__main__":
    d = write_amxd(DEVICE, device_patcher()); print("wrote", os.path.relpath(d, ROOT), os.path.getsize(d), "bytes")
    h = os.path.join(ROOT, "max", "PolyPitch", "help", "polypitch~.maxhelp"); json.dump(help_patcher(), open(h, "w"), indent="\t"); print("wrote", os.path.relpath(h, ROOT))
