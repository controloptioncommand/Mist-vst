#include "Params.h"
#include "Algorithms.h"

namespace mist
{
using APF  = juce::AudioParameterFloat;
using API  = juce::AudioParameterInt;
using APB  = juce::AudioParameterBool;
using APC  = juce::AudioParameterChoice;
using NR   = juce::NormalisableRange<float>;
using Attr = juce::AudioParameterFloatAttributes;

juce::StringArray modSourceNames()
{
    return { "None", "LFO 1", "LFO 2", "Mod Env", "Velocity", "Key", "Mod Wheel", "Aftertouch", "Random" };
}

juce::StringArray modDestNames()
{
    juce::StringArray s { "None", "Pitch", "FM Amount", "Cutoff", "Resonance", "Uni Detune", "Volume", "Pan" };
    for (const char* kind : { "Level", "Pitch", "Scan", "Feedback" })
        for (int i = 0; i < kNumOps; ++i)
            s.add ("Op" + juce::String (i + 1) + " " + kind);
    jassert (s.size() == NumModDests);
    return s;
}

juce::StringArray lfoDivisionNames()
{
    return { "8 bars", "4 bars", "2 bars", "1 bar", "1/2", "1/2T", "1/4", "1/4T", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
}

double lfoDivisionInBeats (int i)
{
    static const double beats[] = { 32, 16, 8, 4, 2, 4.0 / 3.0, 1, 2.0 / 3.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
    return beats[juce::jlimit (0, 12, i)];
}

juce::StringArray algorithmChoiceNames()
{
    juce::StringArray s { "Matrix" };
    for (int i = 0; i < kNumAlgorithms; ++i)
        s.add (juce::String (i + 1) + " " + getAlgorithm (i).name);
    return s;
}

float coarseToRatio (int coarse)
{
    return coarse <= 0 ? 0.5f : (float) coarse;
}

static NR timeRange (float maxSec, float centre)
{
    NR r (0.0f, maxSec, 0.0f);
    r.setSkewForCentre (centre);
    return r;
}

static Attr secondsAttr()
{
    return Attr().withStringFromValueFunction ([] (float v, int)
    {
        if (v < 1.0f) return juce::String (juce::roundToInt (v * 1000.0f)) + " ms";
        return juce::String (v, 2) + " s";
    });
}

static Attr percentAttr()
{
    return Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; });
}

static Attr bipolarPercentAttr()
{
    return Attr().withStringFromValueFunction ([] (float v, int)
    {
        auto p = juce::roundToInt (v * 100.0f);
        return (p > 0 ? "+" : "") + juce::String (p) + "%";
    });
}

static Attr unitAttr (const char* unit, int decimals)
{
    return Attr().withLabel (unit).withStringFromValueFunction ([unit, decimals] (float v, int)
    {
        return juce::String (v, decimals) + " " + unit;
    });
}

static Attr hzAttr()
{
    return Attr().withStringFromValueFunction ([] (float v, int)
    {
        if (v >= 1000.0f) return juce::String (v / 1000.0f, 2) + " kHz";
        return juce::String (v, v < 10.0f ? 2 : 1) + " Hz";
    });
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    auto pid = [] (const juce::String& id) { return juce::ParameterID { id, 1 }; };

    // ---------------- Global
    auto global = std::make_unique<juce::AudioProcessorParameterGroup> ("global", "Global", "|");
    global->addChild (std::make_unique<APC> (pid ("algorithm"), "Algorithm", algorithmChoiceNames(), 1));
    global->addChild (std::make_unique<APF> (pid ("fmAmount"), "FM Amount", NR (0.0f, 4.0f, 0.0f, 0.6f), 1.0f, percentAttr()));
    global->addChild (std::make_unique<APF> (pid ("master"), "Master", NR (-48.0f, 6.0f, 0.0f, 2.0f), -6.0f,
                                             unitAttr ("dB", 1)));
    global->addChild (std::make_unique<APC> (pid ("polyMode"), "Voice Mode", juce::StringArray { "Poly", "Mono", "Legato" }, 0));
    global->addChild (std::make_unique<APF> (pid ("glide"), "Glide", timeRange (5.0f, 0.3f), 0.0f, secondsAttr()));
    global->addChild (std::make_unique<API> (pid ("bendRange"), "Bend Range", 0, 24, 2));
    global->addChild (std::make_unique<API> (pid ("uniVoices"), "Unison", 1, kMaxUnison, 1));
    global->addChild (std::make_unique<APF> (pid ("uniDetune"), "Uni Detune", NR (0.0f, 100.0f, 0.0f, 0.5f), 12.0f,
                                             unitAttr ("ct", 1)));
    global->addChild (std::make_unique<APF> (pid ("uniSpread"), "Uni Spread", NR (0.0f, 1.0f), 0.7f, percentAttr()));
    global->addChild (std::make_unique<APB> (pid ("drone"), "Drone", false));
    global->addChild (std::make_unique<APC> (pid ("interp"), "Interpolation", juce::StringArray { "Linear", "Hermite" }, 1));
    global->addChild (std::make_unique<APC> (pid ("oversampling"), "Oversampling", juce::StringArray { "1x", "2x", "4x" }, 1));
    layout.add (std::move (global));

    // ---------------- Operators
    for (int i = 0; i < kNumOps; ++i)
    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("op" + juce::String (i + 1),
                                                                        "Op " + juce::String (i + 1), "|");
        auto add = [&] (std::unique_ptr<juce::RangedAudioParameter> p) { g->addChild (std::move (p)); };
        auto nm  = [&] (const char* n) { return "Op" + juce::String (i + 1) + " " + n; };

        add (std::make_unique<APB> (pid (opId (i, "on")), nm ("On"), true));
        add (std::make_unique<APC> (pid (opId (i, "mode")), nm ("Mode"), juce::StringArray { "Scrub", "Sample PM", "Grain" }, 0));
        add (std::make_unique<API> (pid (opId (i, "coarse")), nm ("Coarse"), 0, 32, 1, juce::AudioParameterIntAttributes()
                .withStringFromValueFunction ([] (int v, int) { return v == 0 ? juce::String ("0.5") : juce::String (v); })));
        add (std::make_unique<APF> (pid (opId (i, "fine")), nm ("Fine"), NR (0.0f, 1.0f), 0.0f, percentAttr()));
        add (std::make_unique<APF> (pid (opId (i, "detune")), nm ("Detune"), NR (-100.0f, 100.0f), 0.0f, unitAttr ("ct", 1)));
        add (std::make_unique<APB> (pid (opId (i, "fixed")), nm ("Fixed"), false));
        NR fixedRange (0.1f, 10000.0f, 0.0f); fixedRange.setSkewForCentre (200.0f);
        add (std::make_unique<APF> (pid (opId (i, "fixedHz")), nm ("Fixed Freq"), fixedRange, 110.0f, hzAttr()));
        add (std::make_unique<APF> (pid (opId (i, "level")), nm ("Level"), NR (0.0f, 1.0f), i == 0 ? 0.8f : 0.0f, percentAttr()));
        add (std::make_unique<APF> (pid (opId (i, "velSens")), nm ("Vel Sens"), NR (0.0f, 1.0f), 0.5f, percentAttr()));
        add (std::make_unique<APF> (pid (opId (i, "feedback")), nm ("Feedback"), NR (0.0f, 1.0f), 0.0f, percentAttr()));
        add (std::make_unique<APF> (pid (opId (i, "scan")), nm ("Scan"), NR (0.0f, 1.0f), 0.0f, percentAttr()));
        add (std::make_unique<APF> (pid (opId (i, "pan")), nm ("Pan"), NR (-1.0f, 1.0f), 0.0f, bipolarPercentAttr()));
        add (std::make_unique<APF> (pid (opId (i, "out")), nm ("Matrix Out"), NR (0.0f, 1.0f), i == 0 ? 1.0f : 0.0f, percentAttr()));

        // Envelope (DAHDSR) — длинные времена для дронов
        add (std::make_unique<APF> (pid (opId (i, "delay")),   nm ("Delay"),   timeRange (20.0f, 1.0f), 0.0f, secondsAttr()));
        add (std::make_unique<APF> (pid (opId (i, "attack")),  nm ("Attack"),  timeRange (30.0f, 1.0f), 0.005f, secondsAttr()));
        add (std::make_unique<APF> (pid (opId (i, "hold")),    nm ("Hold"),    timeRange (20.0f, 1.0f), 0.0f, secondsAttr()));
        add (std::make_unique<APF> (pid (opId (i, "decay")),   nm ("Decay"),   timeRange (30.0f, 1.0f), 0.6f, secondsAttr()));
        add (std::make_unique<APF> (pid (opId (i, "sustain")), nm ("Sustain"), NR (0.0f, 1.0f), 0.8f, percentAttr()));
        add (std::make_unique<APF> (pid (opId (i, "release")), nm ("Release"), timeRange (30.0f, 1.5f), 0.4f, secondsAttr()));

        // Sample playback
        add (std::make_unique<APF> (pid (opId (i, "start")),     nm ("Start"),      NR (0.0f, 1.0f), 0.0f, percentAttr()));
        add (std::make_unique<APF> (pid (opId (i, "loopStart")), nm ("Loop Start"), NR (0.0f, 1.0f), 0.0f, percentAttr()));
        add (std::make_unique<APF> (pid (opId (i, "loopEnd")),   nm ("Loop End"),   NR (0.0f, 1.0f), 1.0f, percentAttr()));
        add (std::make_unique<APF> (pid (opId (i, "xfade")),     nm ("Loop XFade"), NR (0.0f, 0.5f), 0.1f, percentAttr()));
        add (std::make_unique<APC> (pid (opId (i, "loopMode")),  nm ("Loop Mode"),
                                    juce::StringArray { "Off", "Forward", "Ping-Pong" }, 1));
        add (std::make_unique<APB> (pid (opId (i, "reverse")),   nm ("Reverse"), false));
        add (std::make_unique<API> (pid (opId (i, "root")),      nm ("Root"), 0, 127, 60, juce::AudioParameterIntAttributes()
                .withStringFromValueFunction ([] (int v, int) { return juce::MidiMessage::getMidiNoteName (v, true, true, 4); })));
        add (std::make_unique<APB> (pid (opId (i, "keytrack")),  nm ("Keytrack"), true));

        // Grain
        NR grainSize (0.01f, 1.0f, 0.0f); grainSize.setSkewForCentre (0.12f);
        add (std::make_unique<APF> (pid (opId (i, "grainSize")),    nm ("Grain Size"),    grainSize, 0.12f, secondsAttr()));
        add (std::make_unique<APF> (pid (opId (i, "grainDensity")), nm ("Grain Density"), NR (1.0f, 6.0f), 3.0f,
                                    Attr().withStringFromValueFunction ([] (float v, int) { return "x" + juce::String (v, 1); })));
        add (std::make_unique<APF> (pid (opId (i, "grainSpray")),   nm ("Grain Spray"),   NR (0.0f, 1.0f), 0.1f, percentAttr()));

        layout.add (std::move (g));
    }

    // ---------------- Routing matrix (src -> dst), diagonal = feedback (отдельный параметр)
    auto mat = std::make_unique<juce::AudioProcessorParameterGroup> ("matrix", "FM Matrix", "|");
    for (int s = 0; s < kNumOps; ++s)
        for (int d = 0; d < kNumOps; ++d)
            if (s != d)
                mat->addChild (std::make_unique<APF> (pid (matId (s, d)),
                                                      "Mod " + juce::String (s + 1) + ">" + juce::String (d + 1),
                                                      NR (0.0f, 1.0f), 0.0f, percentAttr()));
    layout.add (std::move (mat));

    // ---------------- Filter
    auto filt = std::make_unique<juce::AudioProcessorParameterGroup> ("filter", "Filter", "|");
    filt->addChild (std::make_unique<APB> (pid ("filterOn"), "Filter On", false));
    filt->addChild (std::make_unique<APC> (pid ("filterType"), "Filter Type",
                                           juce::StringArray { "LP 12", "LP 24", "BP", "HP", "Notch" }, 1));
    NR cutRange (20.0f, 20000.0f, 0.0f); cutRange.setSkewForCentre (1000.0f);
    filt->addChild (std::make_unique<APF> (pid ("cutoff"), "Cutoff", cutRange, 8000.0f, hzAttr()));
    filt->addChild (std::make_unique<APF> (pid ("reso"), "Resonance", NR (0.0f, 1.0f), 0.1f, percentAttr()));
    filt->addChild (std::make_unique<APF> (pid ("filterEnv"), "Filter Env", NR (-1.0f, 1.0f), 0.0f, bipolarPercentAttr()));
    filt->addChild (std::make_unique<APF> (pid ("filterKey"), "Filter Key", NR (0.0f, 1.0f), 0.0f, percentAttr()));
    filt->addChild (std::make_unique<APF> (pid ("drive"), "Drive", NR (0.0f, 1.0f), 0.0f, percentAttr()));
    layout.add (std::move (filt));

    // ---------------- Mod envelope
    auto menv = std::make_unique<juce::AudioProcessorParameterGroup> ("modenv", "Mod Env", "|");
    menv->addChild (std::make_unique<APF> (pid ("menv_attack"),  "ModEnv Attack",  timeRange (30.0f, 1.0f), 0.01f, secondsAttr()));
    menv->addChild (std::make_unique<APF> (pid ("menv_decay"),   "ModEnv Decay",   timeRange (30.0f, 1.0f), 1.0f, secondsAttr()));
    menv->addChild (std::make_unique<APF> (pid ("menv_sustain"), "ModEnv Sustain", NR (0.0f, 1.0f), 0.0f, percentAttr()));
    menv->addChild (std::make_unique<APF> (pid ("menv_release"), "ModEnv Release", timeRange (30.0f, 1.5f), 0.5f, secondsAttr()));
    layout.add (std::move (menv));

    // ---------------- LFOs
    for (int l = 0; l < 2; ++l)
    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("lfo" + juce::String (l + 1), "LFO " + juce::String (l + 1), "|");
        auto nm = [&] (const char* n) { return "LFO" + juce::String (l + 1) + " " + n; };
        g->addChild (std::make_unique<APC> (pid (lfoId (l, "shape")), nm ("Shape"),
                     juce::StringArray { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H", "Smooth Rnd" }, 0));
        NR rateRange (0.01f, 40.0f, 0.0f); rateRange.setSkewForCentre (1.0f);
        g->addChild (std::make_unique<APF> (pid (lfoId (l, "rate")), nm ("Rate"), rateRange, l == 0 ? 0.25f : 2.0f, hzAttr()));
        g->addChild (std::make_unique<APB> (pid (lfoId (l, "sync")), nm ("Sync"), false));
        g->addChild (std::make_unique<APC> (pid (lfoId (l, "div")), nm ("Division"), lfoDivisionNames(), 3));
        layout.add (std::move (g));
    }

    // ---------------- Mod matrix
    auto mm = std::make_unique<juce::AudioProcessorParameterGroup> ("modmatrix", "Mod Matrix", "|");
    for (int s = 0; s < kNumModSlots; ++s)
    {
        auto nm = [&] (const char* n) { return "Mod" + juce::String (s + 1) + " " + n; };
        mm->addChild (std::make_unique<APC> (pid (modId (s, "src")), nm ("Source"), modSourceNames(), 0));
        mm->addChild (std::make_unique<APC> (pid (modId (s, "dst")), nm ("Dest"), modDestNames(), 0));
        mm->addChild (std::make_unique<APF> (pid (modId (s, "amt")), nm ("Amount"), NR (-1.0f, 1.0f), 0.0f, bipolarPercentAttr()));
    }
    layout.add (std::move (mm));

    return layout;
}
} // namespace mist
