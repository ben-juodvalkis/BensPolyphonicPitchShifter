// polypitch~ : PolyPitch as a Max object. A thin wrapper around polypitch::Processor (engine/PolyPitchProcessor.h),
// the same code the plug-in runs.
//
//   inlets   signal left, signal right (leave the right one unconnected for mono)
//   outlets  signal left, signal right
//   @semitones  -12 .. 12 (default -12; also the first argument: [polypitch~ 12])
//   @mix        0 .. 100: 0 = dry only, 50 = both at full level, 100 = shifted only (default)
//   @tone       0 .. 100: how much of the interval-dependent tone curve is applied to the shifted sound (default 100)
//   @response   shifting up only: 0 = fast (default), 1 = balanced (attacks 4 ms later, cleaner), 2 = clean (8 ms later; a full chord's middle note comes out right)
//   @quality    0 = full (default), 1 = lite (about four fifths of the CPU shifting down and under three fifths shifting up; clean chords a little less clean)
//   clear       forget everything heard so far
//
// The shifted sound is mono (the two inputs summed), sent to both outlets; the dry signal passes in stereo and is
// never delayed. Copyright (c) 2026 Ben Juodvalkis. MIT License (see LICENSE).
#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"
#include "PolyPitchProcessor.h"

typedef struct _polypitch
{
    t_pxobject ob;
    polypitch::Processor* proc;
    t_atom_long semitones;
    double mix;
    double tone;
    t_atom_long response;
    t_atom_long quality;
    long stereo;        // is the right inlet connected?
    long clearNext;     // set by "clear", carried out on the audio thread
} t_polypitch;

static t_class* s_polypitch_class = nullptr;

static void polypitch_perform64 (t_polypitch* x, t_object*, double** ins, long, double** outs, long, long sampleframes, long, void*)
{
    polypitch::Processor& p = *x->proc;
    if (x->clearNext) { p.reset(); x->clearNext = 0; }
    p.setSemitones ((int) x->semitones); p.setMix (x->mix / 100.0); p.setTone (x->tone / 100.0); p.setResponse ((int) x->response); p.setLite (x->quality != 0);
    p.process (ins[0], x->stereo ? ins[1] : (const double*) nullptr, outs[0], outs[1], (int) sampleframes);
}

static void polypitch_dsp64 (t_polypitch* x, t_object* dsp64, short* count, double samplerate, long, long)
{
    x->stereo = count[1];
    x->proc->setSemitones ((int) x->semitones); x->proc->setMix (x->mix / 100.0); x->proc->setTone (x->tone / 100.0); x->proc->setResponse ((int) x->response); x->proc->setLite (x->quality != 0);
    x->proc->prepare (samplerate);
    object_method (dsp64, gensym ("dsp_add64"), x, polypitch_perform64, 0, NULL);
}

static void polypitch_clear (t_polypitch* x) { x->clearNext = 1; }

static void polypitch_assist (t_polypitch*, void*, long m, long a, char* s)
{
    if (m == ASSIST_INLET) snprintf_zero (s, 256, a == 0 ? "(signal) left in; messages and attributes" : "(signal) right in (leave unconnected for mono)");
    else snprintf_zero (s, 256, a == 0 ? "(signal) left out" : "(signal) right out");
}

static void polypitch_free (t_polypitch* x)
{
    dsp_free ((t_pxobject*) x);
    delete x->proc;
}

static void* polypitch_new (t_symbol*, long argc, t_atom* argv)
{
    t_polypitch* x = (t_polypitch*) object_alloc (s_polypitch_class);
    if (x == nullptr) return nullptr;
    dsp_setup ((t_pxobject*) x, 2);
    x->ob.z_misc |= Z_NO_INPLACE;                      // the dry signal is read while the outputs are written
    outlet_new (x, "signal"); outlet_new (x, "signal");
    x->proc = new polypitch::Processor();
    x->semitones = -12; x->mix = 100.0; x->tone = 100.0; x->response = 0; x->quality = 0; x->stereo = 0; x->clearNext = 0;
    if (argc > 0 && (atom_gettype (argv) == A_LONG || atom_gettype (argv) == A_FLOAT))
        x->semitones = (t_atom_long) CLAMP (atom_getlong (argv), -12, 12);
    attr_args_process (x, (short) argc, argv);
    return x;
}

void ext_main (void*)
{
    t_class* c = class_new ("polypitch~", (method) polypitch_new, (method) polypitch_free, (long) sizeof (t_polypitch), 0L, A_GIMME, 0);
    class_addmethod (c, (method) polypitch_dsp64, "dsp64", A_CANT, 0);
    class_addmethod (c, (method) polypitch_assist, "assist", A_CANT, 0);
    class_addmethod (c, (method) polypitch_clear, "clear", 0);

    CLASS_ATTR_LONG (c, "semitones", 0, t_polypitch, semitones);
    CLASS_ATTR_FILTER_CLIP (c, "semitones", -12, 12);
    CLASS_ATTR_LABEL (c, "semitones", 0, "Shift in semitones");
    CLASS_ATTR_DOUBLE (c, "mix", 0, t_polypitch, mix);
    CLASS_ATTR_FILTER_CLIP (c, "mix", 0.0, 100.0);
    CLASS_ATTR_LABEL (c, "mix", 0, "Mix (0 = dry, 100 = shifted)");
    CLASS_ATTR_DOUBLE (c, "tone", 0, t_polypitch, tone);
    CLASS_ATTR_FILTER_CLIP (c, "tone", 0.0, 100.0);
    CLASS_ATTR_LABEL (c, "tone", 0, "Tone curve amount");
    CLASS_ATTR_LONG (c, "response", 0, t_polypitch, response);
    CLASS_ATTR_FILTER_CLIP (c, "response", 0, 2);
    CLASS_ATTR_LABEL (c, "response", 0, "Response shifting up (0 = fast, 1 = balanced, 2 = clean)");
    CLASS_ATTR_LONG (c, "quality", 0, t_polypitch, quality);
    CLASS_ATTR_FILTER_CLIP (c, "quality", 0, 1);
    CLASS_ATTR_LABEL (c, "quality", 0, "Quality (0 = full, 1 = lite)");

    class_dspinit (c);
    class_register (CLASS_BOX, c);
    s_polypitch_class = c;
}
