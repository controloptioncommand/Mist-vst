#include "FactoryPresets.h"
#include "Params.h"

namespace mist
{
namespace
{
struct Builder
{
    FactoryPreset p;
    explicit Builder (const char* name) { p.name = name; }
    Builder& set (const juce::String& id, float v) { p.values.push_back ({ id, v }); return *this; }
    Builder& op (int i, const char* name, float v) { return set (opId (i - 1, name), v); }
    Builder& env (int i, float a, float d, float s, float r)
    {
        return op (i, "attack", a).op (i, "decay", d).op (i, "sustain", s).op (i, "release", r);
    }
    Builder& allEnv (float a, float d, float s, float r)
    {
        for (int i = 1; i <= kNumOps; ++i) env (i, a, d, s, r);
        return *this;
    }
    Builder& slot (int n, int src, int dst, float amt)
    {
        return set (modId (n - 1, "src"), (float) src).set (modId (n - 1, "dst"), (float) dst).set (modId (n - 1, "amt"), amt);
    }
    operator FactoryPreset() const { return p; }
};

// Wave-кадры встроенной таблицы: 0 Sine, 1 Sine+Oct, 2 Tri, 3 Square, 4 Saw, 5 Pulse25, 6 Pulse10, 7 Formant
constexpr float frame (int f) { return (float) f / 7.0f; }
} // namespace

const std::vector<FactoryPreset>& getFactoryPresets()
{
    static const std::vector<FactoryPreset> presets =
    {
        Builder ("Init FM")
            .set ("algorithm", 1)
            .op (1, "level", 0.8f).op (2, "level", 0.45f).op (2, "coarse", 2),

        Builder ("Glass Pad")
            .set ("algorithm", 6).set ("uniVoices", 4).set ("uniDetune", 14).set ("uniSpread", 0.8f)
            .set ("filterOn", 1).set ("cutoff", 6500).set ("reso", 0.15f)
            .op (1, "level", 0.75f).op (3, "level", 0.6f).op (5, "level", 0.5f)
            .op (3, "coarse", 2).op (5, "coarse", 4).op (5, "detune", 6)
            .op (2, "level", 0.38f).op (2, "coarse", 3)
            .op (4, "level", 0.3f).op (4, "coarse", 7)
            .op (6, "level", 0.25f).op (6, "coarse", 1).op (6, "fine", 0.5f)
            .allEnv (2.2f, 3.0f, 0.75f, 4.5f)
            .set (lfoId (0, "rate"), 0.12f)
            .slot (1, SrcLfo1, DstFMAmount, 0.25f)
            .slot (2, SrcModWheel, DstOpLevel0 + 1, 0.4f),

        Builder ("Dark Ring Drone")
            .set ("algorithm", 24).set ("uniVoices", 6).set ("uniDetune", 22).set ("uniSpread", 1.0f)
            .set ("fmAmount", 0.8f)
            .set ("filterOn", 1).set ("filterType", FiltLP24).set ("cutoff", 1800).set ("reso", 0.3f)
            .op (1, "level", 0.85f).op (1, "coarse", 0)
            .op (2, "level", 0.35f).op (2, "coarse", 1)
            .op (3, "level", 0.3f).op (3, "coarse", 1).op (3, "detune", -9)
            .op (4, "level", 0.28f).op (4, "coarse", 2).op (4, "scan", frame (4))
            .op (5, "level", 0.22f).op (5, "coarse", 3)
            .op (6, "level", 0.3f).op (6, "coarse", 0).op (6, "feedback", 0.4f)
            .allEnv (6.0f, 8.0f, 1.0f, 8.0f)
            .set (lfoId (0, "shape"), LfoSmoothRandom).set (lfoId (0, "rate"), 0.08f)
            .set (lfoId (1, "shape"), LfoTriangle).set (lfoId (1, "rate"), 0.03f)
            .slot (1, SrcLfo1, DstFMAmount, 0.35f)
            .slot (2, SrcLfo2, DstCutoff, 0.3f)
            .slot (3, SrcLfo1, DstOpScan0 + 3, 0.5f),

        Builder ("Metal Bell")
            .set ("algorithm", 10)
            .op (1, "level", 0.85f)
            .op (2, "level", 0.4f).op (2, "coarse", 3).op (2, "fine", 0.17f)
            .op (3, "level", 0.3f).op (3, "coarse", 7).op (3, "detune", 11)
            .op (4, "level", 0.22f).op (4, "fixed", 1).op (4, "fixedHz", 1870)
            .op (5, "level", 0.18f).op (5, "coarse", 11)
            .op (6, "level", 0.0f)
            .allEnv (0.002f, 6.0f, 0.0f, 3.0f)
            .env (3, 0.001f, 0.8f, 0.0f, 0.5f).env (4, 0.001f, 0.25f, 0.0f, 0.3f),

        Builder ("Breathing Swarm")
            .set ("algorithm", 26).set ("uniVoices", 8).set ("uniDetune", 35).set ("uniSpread", 1.0f)
            .set ("fmAmount", 0.6f)
            .op (1, "level", 0.6f).op (3, "level", 0.6f).op (5, "level", 0.6f)
            .op (2, "level", 0.3f).op (2, "coarse", 1).op (2, "scan", frame (2))
            .op (4, "level", 0.3f).op (4, "coarse", 2)
            .op (6, "level", 0.3f).op (6, "coarse", 0)
            .op (1, "pan", -0.6f).op (5, "pan", 0.6f)
            .allEnv (4.0f, 4.0f, 0.9f, 6.0f)
            .set (lfoId (0, "shape"), LfoSmoothRandom).set (lfoId (0, "rate"), 0.4f)
            .set (lfoId (1, "shape"), LfoSine).set (lfoId (1, "rate"), 0.07f)
            .slot (1, SrcLfo1, DstFMAmount, 0.5f)
            .slot (2, SrcLfo2, DstUniDetune, 0.6f)
            .slot (3, SrcLfo2, DstPan, 0.3f),

        Builder ("Formant Choir")
            .set ("algorithm", 21).set ("uniVoices", 5).set ("uniDetune", 18).set ("uniSpread", 0.9f)
            .set ("filterOn", 1).set ("filterType", FiltBP).set ("cutoff", 1200).set ("reso", 0.2f).set ("filterKey", 0.5f)
            .op (1, "level", 0.8f).op (1, "scan", frame (7))
            .op (4, "level", 0.65f).op (4, "scan", frame (5)).op (4, "coarse", 2)
            .op (2, "level", 0.3f).op (2, "coarse", 1).op (3, "level", 0.2f).op (3, "coarse", 3)
            .op (5, "level", 0.25f).op (5, "coarse", 1).op (6, "level", 0.15f).op (6, "coarse", 5)
            .allEnv (1.2f, 2.0f, 0.85f, 2.5f)
            .set (lfoId (0, "rate"), 5.2f)
            .slot (1, SrcLfo1, DstPitch, 0.006f)
            .slot (2, SrcModWheel, DstOpScan0, -0.6f),

        Builder ("Chaos Engine")
            .set ("algorithm", 32).set ("fmAmount", 1.4f).set ("polyMode", PolyMono).set ("glide", 0.25f)
            .set ("filterOn", 1).set ("cutoff", 3000).set ("reso", 0.45f).set ("drive", 0.4f).set ("filterEnv", 0.4f)
            .set ("menv_attack", 0.01f).set ("menv_decay", 1.2f).set ("menv_sustain", 0.2f)
            .op (1, "level", 0.8f).op (4, "level", 0.7f)
            .op (2, "level", 0.4f).op (2, "coarse", 1).op (3, "level", 0.35f).op (3, "coarse", 2)
            .op (5, "level", 0.4f).op (5, "coarse", 3).op (6, "level", 0.3f).op (6, "coarse", 0).op (6, "feedback", 0.6f)
            .allEnv (0.01f, 1.0f, 0.7f, 0.6f)
            .set (lfoId (0, "shape"), LfoSampleHold).set (lfoId (0, "sync"), 1).set (lfoId (0, "div"), 10)
            .slot (1, SrcLfo1, DstOpLevel0 + 4, 0.35f)
            .slot (2, SrcModEnv, DstFMAmount, 0.5f),

        Builder ("Hollow Sub")
            .set ("algorithm", 7).set ("polyMode", PolyLegato).set ("glide", 0.08f)
            .op (1, "level", 0.85f).op (1, "coarse", 0)
            .op (2, "level", 0.4f).op (2, "scan", frame (2))
            .op (3, "level", 0.2f).op (3, "coarse", 2)
            .op (5, "level", 0.3f).op (5, "coarse", 1).op (6, "level", 0.25f).op (6, "coarse", 1).op (6, "feedback", 0.3f)
            .op (4, "level", 0.4f)
            .allEnv (0.01f, 0.8f, 0.8f, 0.3f)
            .slot (1, SrcVelocity, DstOpLevel0 + 3, 0.3f),
    };
    return presets;
}
} // namespace mist
