#include "FactoryPresets.h"
#include "Params.h"
#include <map>

// 100 шаблонов, по 10 на характер. Семпл кладётся в Op 1 (несущая во всех алгоритмах),
// остальное — обвязка, которая задаёт характер и сохраняется с любым семплом.
//
// Правила характера (по классике FM: индекс модуляции = яркость, целые отношения = гармонично,
// дробные = металл/колокол, огибающая индекса следует за громкостью = «медь»):
//   Airy     — медленная атака, малый индекс, широкий унисон, высокий модулятор с обратной связью как «дыхание»
//   Soft     — малый индекс, отношения 1:1 и 1:2, мягкий фильтр, скруглённые огибающие
//   Warm     — средний индекс, обратная связь на модуляторе (пила), LP24 и лёгкий драйв
//   Hollow   — отношения 1:2 и 1:4 (нечётные гармоники), полосовой фильтр
//   Glassy   — высокие отношения (7, 11, 14) с быстро затухающим индексом — «зубцы» как у электропиано
//   Bright   — большой индекс без фильтра, быстрая атака
//   Hard     — большой индекс, обратная связь, драйв, индекс следует за огибающей
//   Metallic — дробные отношения и фиксированные частоты, долгий спад без sustain
//   Dark     — низкий срез, суб-оператор 0.5, малый индекс
//   Moving   — LFO на позицию, глубину FM, срез и панораму

namespace mist
{
namespace
{
// Wave-кадры встроенной таблицы: 0 Sine, 1 Sine+Oct, 2 Tri, 3 Square, 4 Saw, 5 Pulse25, 6 Pulse10, 7 Formant
constexpr float frame (int f) { return (float) f / 7.0f; }
enum Frame { Sine = 0, SineOct, Tri, Sq, Saw, Pulse25, Pulse10, Formant };

struct B
{
    FactoryPreset p;
    B (const char* name, const char* category) { p.name = name; p.category = category; }

    B& set (const juce::String& id, float v) { p.values.push_back ({ id, v }); return *this; }
    B& op (int i, const char* name, float v) { return set (opId (i - 1, name), v); }

    B& algo (int a)                       { return set ("algorithm", (float) a); }
    B& fm (float v)                       { return set ("fmAmount", v); }
    B& mode (int i, int m)                { return op (i, "mode", (float) m); }
    B& lvl (int i, float v)               { return op (i, "level", v); }
    B& ratio (int i, int coarse, float fine = 0.0f) { return op (i, "coarse", (float) coarse).op (i, "fine", fine); }
    B& det (int i, float cents)           { return op (i, "detune", cents); }
    B& fixedHz (int i, float hz)          { return op (i, "fixed", 1.0f).op (i, "fixedHz", hz); }
    B& fb (int i, float v)                { return op (i, "feedback", v); }
    B& scan (int i, int f)                { return op (i, "scan", frame (f)); }
    B& pos (int i, float v)               { return op (i, "scan", v); }
    B& pan (int i, float v)               { return op (i, "pan", v); }
    B& vel (int i, float v)               { return op (i, "velSens", v); }
    B& grain (int i, float size, float density, float spray)
    {
        return op (i, "grainSize", size).op (i, "grainDensity", density).op (i, "grainSpray", spray);
    }
    B& env (int i, float a, float d, float s, float r)
    {
        return op (i, "attack", a).op (i, "decay", d).op (i, "sustain", s).op (i, "release", r);
    }
    B& allEnv (float a, float d, float s, float r)
    {
        for (int i = 1; i <= kNumOps; ++i) env (i, a, d, s, r);
        return *this;
    }
    B& curve (int i, float a, float d, float r) { return op (i, "aCurve", a).op (i, "dCurve", d).op (i, "rCurve", r); }
    B& allCurve (float a, float d, float r)
    {
        for (int i = 1; i <= kNumOps; ++i) curve (i, a, d, r);
        return *this;
    }
    B& filt (int type, float cutoff, float reso, float envAmt = 0.0f, float key = 0.0f, float drive = 0.0f)
    {
        return set ("filterOn", 1).set ("filterType", (float) type).set ("cutoff", cutoff).set ("reso", reso)
              .set ("filterEnv", envAmt).set ("filterKey", key).set ("drive", drive);
    }
    B& uni (int voices, float detune, float spread)
    {
        return set ("uniVoices", (float) voices).set ("uniDetune", detune).set ("uniSpread", spread);
    }
    B& menv (float a, float d, float s, float r)
    {
        return set ("menv_attack", a).set ("menv_decay", d).set ("menv_sustain", s).set ("menv_release", r);
    }
    B& lfo (int n, int shape, float rate)  { return set (lfoId (n - 1, "shape"), (float) shape).set (lfoId (n - 1, "rate"), rate); }
    B& lfoSync (int n, int shape, int div) { return set (lfoId (n - 1, "shape"), (float) shape).set (lfoId (n - 1, "sync"), 1).set (lfoId (n - 1, "div"), (float) div); }
    B& slot (int n, int src, int dst, float amt)
    {
        return set (modId (n - 1, "src"), (float) src).set (modId (n - 1, "dst"), (float) dst).set (modId (n - 1, "amt"), amt);
    }
    B& mono (bool legato, float glide)    { return set ("polyMode", (float) (legato ? PolyLegato : PolyMono)).set ("glide", glide); }
    B& master (float db)                  { return set ("master", db); }
    operator FactoryPreset() const { return p; }
};

// LFO-деления: 0 8 bars, 1 4 bars, 2 2 bars, 3 1 bar, 4 1/2, 5 1/2T, 6 1/4, 7 1/4T, 8 1/8, 9 1/8T, 10 1/16, 11 1/16T, 12 1/32
constexpr int OpLvl (int op)   { return DstOpLevel0 + op - 1; }
constexpr int OpPitch (int op) { return DstOpPitch0 + op - 1; }
constexpr int OpScan (int op)  { return DstOpScan0 + op - 1; }
constexpr int OpFb (int op)    { return DstOpFeedback0 + op - 1; }

// ---------------------------------------------------------------- базы характеров
B airy (const char* n)
{
    return B (n, "Airy").fm (0.5f).uni (4, 16.0f, 0.9f)
        .allEnv (1.4f, 3.0f, 0.85f, 3.5f).allCurve (-0.2f, 0.4f, 0.5f)
        .filt (FiltLP12, 6000.0f, 0.12f, 0.0f, 0.3f)
        .lfo (1, LfoSine, 5.0f).slot (1, SrcLfo1, DstPitch, 0.005f);   // лёгкое вибрато
}

B soft (const char* n)
{
    return B (n, "Soft").fm (0.45f).uni (2, 7.0f, 0.6f)
        .allEnv (0.12f, 1.6f, 0.8f, 1.2f).allCurve (0.3f, 0.5f, 0.6f)
        .filt (FiltLP24, 3200.0f, 0.08f, 0.15f, 0.4f);
}

B warm (const char* n)
{
    return B (n, "Warm").fm (0.75f).uni (3, 10.0f, 0.7f)
        .allEnv (0.03f, 1.2f, 0.75f, 0.8f)
        .filt (FiltLP24, 2600.0f, 0.22f, 0.25f, 0.5f, 0.15f)
        .menv (0.02f, 0.9f, 0.2f, 0.8f);
}

B hollow (const char* n)
{
    return B (n, "Hollow").fm (0.7f).uni (1, 0.0f, 0.5f)
        .allEnv (0.04f, 0.9f, 0.8f, 0.6f)
        .filt (FiltBP, 1400.0f, 0.18f, 0.0f, 0.6f);
}

B glassy (const char* n)
{
    return B (n, "Glassy").fm (0.9f).uni (2, 5.0f, 0.6f)
        .allEnv (0.003f, 2.8f, 0.25f, 1.4f).allCurve (0.0f, 0.6f, 0.6f);
}

B bright (const char* n)
{
    return B (n, "Bright").fm (1.2f).uni (3, 14.0f, 0.8f)
        .allEnv (0.005f, 1.0f, 0.8f, 0.4f).allCurve (0.2f, 0.5f, 0.5f);
}

B hard (const char* n)
{
    return B (n, "Hard").fm (1.6f)
        .allEnv (0.002f, 0.6f, 0.75f, 0.25f).allCurve (0.5f, 0.6f, 0.6f)
        .filt (FiltLP24, 5000.0f, 0.3f, 0.35f, 0.3f, 0.45f)
        .menv (0.003f, 0.5f, 0.3f, 0.3f).slot (8, SrcModEnv, DstFMAmount, 0.4f);   // индекс следует за огибающей
}

B metallic (const char* n)
{
    return B (n, "Metallic").fm (1.0f)
        .allEnv (0.002f, 5.0f, 0.0f, 3.0f).allCurve (0.0f, 0.55f, 0.55f);
}

B dark (const char* n)
{
    return B (n, "Dark").fm (0.45f).uni (2, 9.0f, 0.6f)
        .allEnv (0.5f, 3.0f, 0.85f, 2.5f).allCurve (-0.1f, 0.5f, 0.5f)
        .filt (FiltLP24, 900.0f, 0.2f, 0.15f, 0.35f, 0.1f);
}

B moving (const char* n)
{
    return B (n, "Moving").fm (0.8f).uni (3, 12.0f, 0.8f)
        .allEnv (0.3f, 2.0f, 0.8f, 2.0f)
        .filt (FiltLP12, 4500.0f, 0.2f, 0.0f, 0.3f);
}
} // namespace

juce::StringArray getTemplateCategories()
{
    return { "Airy", "Soft", "Warm", "Hollow", "Glassy", "Bright", "Hard", "Metallic", "Dark", "Moving" };
}

static std::vector<FactoryPreset> buildTemplates();

// Выравнивание громкости (дБ к Master −6): замер рендер-тестом (MIST_TEMPLATE_LEVELS=1), аккорд из 4 нот,
// среднее «без семпла» и «с пилой». Тянущиеся: RMS установившегося звука (7–9 с) ≈ −16 дБ, но первые 2 с
// не громче −18 дБ; ударные (затухают до тишины): RMS первых 2 с ≈ −21 дБ.
static float levelTrim (const juce::String& name)
{
    static const std::map<juce::String, float> trims =
    {
        { "Airy Veil", 3.0f },
        { "Airy Breath", 5.0f },
        { "Airy Halo", -5.0f },
        { "Airy Drift", 1.0f },
        { "Airy Cloud", 0.0f },
        { "Airy Whisper", 5.0f },
        { "Airy Choir", 3.0f },
        { "Airy Glow", 1.0f },
        { "Airy Horizon", -4.0f },
        { "Airy Swarm", -1.0f },
        { "Soft Keys", -1.0f },
        { "Soft Pad", 0.0f },
        { "Soft Felt", -1.0f },   // ударный
        { "Soft Flute", -1.0f },
        { "Soft Bloom", -6.0f },
        { "Soft Cushion", -5.0f },
        { "Soft Mallet", 2.0f },   // ударный
        { "Soft Hum", -5.0f },
        { "Soft Lull", -2.0f },
        { "Soft Velvet", -6.0f },
        { "Warm Strings", -8.0f },
        { "Warm Brass", -10.0f },
        { "Warm Analog", -10.0f },
        { "Warm Organ", -11.0f },
        { "Warm Pad", -8.0f },
        { "Warm Bass", -7.0f },
        { "Warm Lead", -9.0f },
        { "Warm Tape", -11.0f },
        { "Warm Ember", -9.0f },
        { "Warm Honey", -11.0f },
        { "Hollow Reed", 3.0f },
        { "Hollow Pipe", 2.0f },
        { "Hollow Clarinet", -1.0f },
        { "Hollow Wood", 11.0f },   // ударный
        { "Hollow Box", -4.0f },
        { "Hollow Vox", 7.0f },
        { "Hollow Tube", 0.0f },
        { "Hollow Square", -3.0f },
        { "Hollow Ocarina", 6.0f },
        { "Hollow Sub", 6.0f },
        { "Glassy Tines", 1.0f },
        { "Glassy Keys", 0.0f },
        { "Glassy Harp", 3.0f },   // ударный
        { "Glassy Celesta", -2.0f },   // ударный
        { "Glassy Shards", 6.0f },
        { "Glassy Pluck", 6.0f },   // ударный
        { "Glassy Mallet", 5.0f },   // ударный
        { "Glassy Pad", -1.0f },
        { "Glassy Rain", 4.0f },
        { "Glassy Chime", -4.0f },   // ударный
        { "Bright Lead", -2.0f },
        { "Bright Stab", 8.0f },   // ударный
        { "Bright Saw", -2.0f },
        { "Bright Brass", -6.0f },
        { "Bright Pluck", 9.0f },   // ударный
        { "Bright Sync", -5.0f },
        { "Bright Chord", 3.0f },
        { "Bright Supersaw", -4.0f },
        { "Bright Arp", 10.0f },   // ударный
        { "Bright Spark", -2.0f },
        { "Hard Bass", -6.0f },
        { "Hard Reese", -5.0f },
        { "Hard Stab", 2.0f },   // ударный
        { "Hard Growl", -6.0f },
        { "Hard Lead", -4.0f },
        { "Hard Hit", 2.0f },   // ударный
        { "Hard Distort", -9.0f },
        { "Hard Grit", -9.0f },
        { "Hard Chaos", -4.0f },
        { "Hard Punch", -12.0f },
        { "Metallic Bell", -6.0f },   // ударный
        { "Metallic Gong", -7.0f },   // ударный
        { "Metallic Tine", 1.0f },   // ударный
        { "Metallic Plate", -1.0f },   // ударный
        { "Metallic Clang", 0.0f },   // ударный
        { "Metallic Bowl", -2.0f },   // ударный
        { "Metallic Ring", -4.0f },
        { "Metallic Spring", 1.0f },   // ударный
        { "Metallic Anvil", 3.0f },   // ударный
        { "Metallic Wire", -4.0f },   // ударный
        { "Dark Drone", -8.0f },
        { "Dark Sub", -8.0f },
        { "Dark Pad", -9.0f },
        { "Dark Hum", -10.0f },
        { "Dark Cello", -6.0f },
        { "Dark Rumble", -12.0f },
        { "Dark Fog", 5.0f },
        { "Dark Night", -5.0f },
        { "Dark Ring", -6.0f },
        { "Dark Undertow", -10.0f },
        { "Moving Waves", 0.0f },
        { "Moving Pulse", -4.0f },
        { "Moving Swirl", 2.0f },
        { "Moving Flicker", 1.0f },
        { "Moving Wobble", -4.0f },
        { "Moving Gate", -6.0f },
        { "Moving Shimmer", 8.0f },
        { "Moving Morph", -5.0f },
        { "Moving Orbit", -1.0f },
        { "Moving Stutter", -1.0f },
    };
    auto it = trims.find (name);
    return it != trims.end() ? it->second : 0.0f;
}

const std::vector<FactoryPreset>& getFactoryPresets()
{
    static const std::vector<FactoryPreset> presets = []
    {
        std::vector<FactoryPreset> list = buildTemplates();
        for (auto& t : list) t.values.push_back ({ "master", -6.0f + levelTrim (t.name) });
        return list;
    }();
    return presets;
}

static std::vector<FactoryPreset> buildTemplates()
{
    return
    {
        // ================================================================ Airy
        airy ("Airy Veil")                                  // зёрна семпла над тихой синусной подушкой
            .algo (6).mode (1, OpModeGrain).pos (1, 0.25f).grain (1, 0.18f, 4.0f, 0.35f).scan (1, Formant)
            .lvl (1, 0.85f).lvl (2, 0.18f).ratio (2, 1)
            .lvl (3, 0.45f).ratio (3, 2).lvl (4, 0.12f).ratio (4, 11).fb (4, 0.7f)
            .lvl (5, 0.35f).ratio (5, 1).det (5, 7.0f).lvl (6, 0.15f).ratio (6, 3)
            .lfo (2, LfoSmoothRandom, 0.15f).slot (2, SrcLfo2, OpScan (1), 0.25f),

        airy ("Airy Breath")                                // дыхание: высокий модулятор с обратной связью
            .algo (3).mode (1, OpModeWave).scan (1, Formant)
            .lvl (1, 0.85f).lvl (2, 0.14f).ratio (2, 13).fb (2, 0.85f)
            .lvl (3, 0.5f).ratio (3, 1).lvl (4, 0.25f).ratio (4, 2).lvl (5, 0.1f).ratio (5, 9).fb (5, 0.6f)
            .env (2, 0.9f, 2.0f, 0.6f, 3.0f)
            .filt (FiltBP, 2400.0f, 0.15f, 0.0f, 0.5f),

        airy ("Airy Halo")
            .algo (12).mode (1, OpModeWave).scan (1, SineOct)
            .lvl (1, 0.9f).lvl (2, 0.2f).ratio (2, 2).lvl (3, 0.15f).ratio (3, 3).lvl (4, 0.08f).ratio (4, 14).fb (4, 0.5f)
            .lvl (5, 0.12f).ratio (5, 4).lvl (6, 0.06f).ratio (6, 7)
            .uni (6, 22.0f, 1.0f).allEnv (2.2f, 4.0f, 0.9f, 5.0f),

        airy ("Airy Drift")
            .algo (6).mode (1, OpModeGrain).pos (1, 0.5f).grain (1, 0.3f, 3.0f, 0.6f).scan (1, Tri)
            .lvl (1, 0.8f).lvl (2, 0.12f).ratio (2, 2)
            .lvl (3, 0.4f).ratio (3, 1).det (3, -9.0f).lvl (4, 0.15f).ratio (4, 3)
            .lvl (5, 0.3f).ratio (5, 2).det (5, 9.0f).lvl (6, 0.08f).ratio (6, 12).fb (6, 0.8f)
            .lfo (2, LfoSine, 0.05f).slot (2, SrcLfo2, OpScan (1), 0.4f).slot (3, SrcLfo2, DstPan, 0.3f),

        airy ("Airy Cloud")
            .algo (8).mode (1, OpModeWave).scan (1, Sine)
            .lvl (1, 0.8f).lvl (2, 0.5f).ratio (2, 1).det (2, -6.0f).lvl (3, 0.45f).ratio (3, 2).det (3, 6.0f)
            .lvl (4, 0.35f).ratio (4, 1).det (4, 12.0f).lvl (5, 0.4f).ratio (5, 3).lvl (6, 0.12f).ratio (6, 9).fb (6, 0.7f)
            .uni (5, 25.0f, 1.0f).allEnv (3.0f, 4.0f, 0.95f, 6.0f).filt (FiltLP12, 4500.0f, 0.1f, 0.0f, 0.2f),

        airy ("Airy Whisper")                               // почти шум: семпл в зёрнах и шипящий модулятор
            .algo (10).mode (1, OpModeGrain).pos (1, 0.1f).grain (1, 0.06f, 5.0f, 0.8f).scan (1, Formant)
            .lvl (1, 0.85f).lvl (2, 0.1f).ratio (2, 15).fb (2, 0.9f).lvl (3, 0.08f).ratio (3, 7).fb (3, 0.6f)
            .filt (FiltHP, 900.0f, 0.1f, 0.0f, 0.4f).uni (3, 30.0f, 1.0f)
            .allEnv (0.8f, 2.0f, 0.7f, 2.5f),

        airy ("Airy Choir")                                 // бывший Formant Choir
            .algo (21).mode (1, OpModeWave).scan (1, Formant).uni (5, 18.0f, 0.9f)
            .filt (FiltBP, 1200.0f, 0.2f, 0.0f, 0.5f)
            .lvl (1, 0.8f).lvl (4, 0.65f).scan (4, Pulse25).ratio (4, 2)
            .lvl (2, 0.3f).ratio (2, 1).lvl (3, 0.2f).ratio (3, 3)
            .lvl (5, 0.25f).ratio (5, 1).lvl (6, 0.15f).ratio (6, 5)
            .allEnv (1.2f, 2.0f, 0.85f, 2.5f).lfo (1, LfoSine, 5.2f)
            .slot (1, SrcLfo1, DstPitch, 0.006f).slot (2, SrcModWheel, OpScan (1), -0.6f),

        airy ("Airy Glow")
            .algo (6).mode (1, OpModeWave).scan (1, SineOct)
            .lvl (1, 0.85f).lvl (2, 0.25f).ratio (2, 1).fb (2, 0.25f)
            .lvl (3, 0.4f).ratio (3, 4).lvl (4, 0.15f).ratio (4, 1)
            .lvl (5, 0.3f).ratio (5, 6).lvl (6, 0.1f).ratio (6, 1)
            .allEnv (1.8f, 3.0f, 0.8f, 4.0f).env (3, 2.5f, 3.0f, 0.7f, 5.0f).env (5, 3.0f, 3.0f, 0.6f, 5.0f),

        airy ("Airy Horizon")
            .algo (4).mode (1, OpModeSample).scan (1, Tri)
            .lvl (1, 0.85f).lvl (2, 0.12f).ratio (2, 1).lvl (3, 0.08f).ratio (3, 2)
            .lvl (4, 0.5f).ratio (4, 1).det (4, -10.0f).lvl (5, 0.2f).ratio (5, 1).lvl (6, 0.1f).ratio (6, 11).fb (6, 0.75f)
            .uni (6, 20.0f, 1.0f).allEnv (2.5f, 5.0f, 0.9f, 6.0f)
            .lfo (2, LfoTriangle, 0.06f).slot (2, SrcLfo2, DstCutoff, 0.2f),

        airy ("Airy Swarm")                                 // бывший Breathing Swarm
            .algo (26).uni (8, 35.0f, 1.0f).fm (0.6f).mode (1, OpModeWave)
            .lvl (1, 0.6f).lvl (3, 0.6f).lvl (5, 0.6f)
            .lvl (2, 0.3f).ratio (2, 1).scan (2, Tri).lvl (4, 0.3f).ratio (4, 2).lvl (6, 0.3f).ratio (6, 0)
            .pan (1, -0.6f).pan (5, 0.6f).allEnv (4.0f, 4.0f, 0.9f, 6.0f)
            .lfo (1, LfoSmoothRandom, 0.4f).lfo (2, LfoSine, 0.07f)
            .slot (1, SrcLfo1, DstFMAmount, 0.5f).slot (2, SrcLfo2, DstUniDetune, 0.6f).slot (3, SrcLfo2, DstPan, 0.3f),

        // ================================================================ Soft
        soft ("Soft Keys")
            .algo (6).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.3f).ratio (2, 1).env (2, 0.005f, 1.2f, 0.3f, 0.8f)
            .lvl (3, 0.35f).ratio (3, 1).lvl (4, 0.25f).ratio (4, 1).env (4, 0.005f, 0.8f, 0.2f, 0.6f)
            .allEnv (0.008f, 2.5f, 0.55f, 0.9f).vel (1, 0.7f).vel (3, 0.7f),

        soft ("Soft Pad")
            .algo (8).mode (1, OpModeWave).scan (1, Sine)
            .lvl (1, 0.8f).lvl (2, 0.4f).ratio (2, 1).det (2, -5.0f).lvl (3, 0.35f).ratio (3, 2)
            .lvl (4, 0.3f).ratio (4, 1).det (4, 5.0f).lvl (5, 0.3f).lvl (6, 0.15f).ratio (6, 1)
            .uni (3, 9.0f, 0.8f).allEnv (0.9f, 2.5f, 0.9f, 2.5f),

        soft ("Soft Felt")                                  // приглушённое пианино на войлоке
            .algo (3).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.22f).ratio (2, 1).env (2, 0.004f, 0.6f, 0.0f, 0.4f)
            .lvl (3, 0.35f).ratio (3, 2).lvl (4, 0.18f).ratio (4, 1)
            .allEnv (0.01f, 3.0f, 0.0f, 0.6f).filt (FiltLP24, 1800.0f, 0.05f, 0.3f, 0.6f)
            .menv (0.003f, 0.6f, 0.0f, 0.4f),

        soft ("Soft Flute")
            .algo (6).mode (1, OpModeWave).scan (1, Tri)
            .lvl (1, 0.85f).lvl (2, 0.18f).ratio (2, 1).lvl (3, 0.25f).ratio (3, 2)
            .lvl (4, 0.08f).ratio (4, 13).fb (4, 0.8f).allEnv (0.15f, 1.0f, 0.85f, 0.4f)
            .lfo (1, LfoSine, 4.8f).slot (1, SrcLfo1, DstPitch, 0.004f).slot (2, SrcModWheel, OpLvl (2), 0.3f),

        soft ("Soft Bloom")
            .algo (12).mode (1, OpModeWave).scan (1, SineOct)
            .lvl (1, 0.85f).lvl (2, 0.25f).ratio (2, 2).lvl (3, 0.15f).ratio (3, 1).lvl (4, 0.1f).ratio (4, 3)
            .env (2, 1.5f, 2.0f, 0.7f, 2.0f).allEnv (0.4f, 2.0f, 0.85f, 2.0f).env (2, 1.6f, 2.0f, 0.8f, 2.0f)
            .uni (3, 8.0f, 0.8f),

        soft ("Soft Cushion")
            .algo (4).mode (1, OpModeWave).scan (1, Sine)
            .lvl (1, 0.8f).lvl (2, 0.3f).ratio (2, 1).lvl (3, 0.15f).ratio (3, 2)
            .lvl (4, 0.6f).ratio (4, 0).lvl (5, 0.2f).ratio (5, 1).lvl (6, 0.1f).ratio (6, 2)
            .allEnv (0.3f, 2.0f, 0.9f, 1.8f).filt (FiltLP24, 2200.0f, 0.1f, 0.0f, 0.4f),

        soft ("Soft Mallet")
            .algo (3).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.3f).ratio (2, 4).env (2, 0.002f, 0.25f, 0.0f, 0.2f)
            .lvl (3, 0.4f).ratio (3, 1).lvl (4, 0.2f).ratio (4, 3).env (4, 0.002f, 0.3f, 0.0f, 0.2f)
            .allEnv (0.003f, 1.8f, 0.0f, 1.0f).allCurve (0.0f, 0.7f, 0.6f).vel (2, 0.8f),

        soft ("Soft Hum")
            .algo (2).mode (1, OpModeWave).scan (1, Sine)
            .lvl (1, 0.85f).lvl (2, 0.45f).ratio (2, 1).lvl (3, 0.2f).ratio (3, 1).lvl (4, 0.1f).ratio (4, 2)
            .allEnv (0.25f, 2.0f, 0.9f, 1.5f).filt (FiltLP12, 1600.0f, 0.1f, 0.0f, 0.5f),

        soft ("Soft Lull")
            .algo (6).mode (1, OpModeGrain).pos (1, 0.3f).grain (1, 0.25f, 3.0f, 0.2f)
            .lvl (1, 0.85f).lvl (2, 0.1f).ratio (2, 1).lvl (3, 0.35f).ratio (3, 1).det (3, 4.0f)
            .lvl (4, 0.12f).ratio (4, 2).allEnv (0.7f, 3.0f, 0.85f, 3.0f)
            .lfo (2, LfoSine, 0.1f).slot (2, SrcLfo2, OpScan (1), 0.15f),

        soft ("Soft Velvet")
            .algo (21).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.2f).ratio (2, 1).lvl (3, 0.12f).ratio (3, 2)
            .lvl (4, 0.4f).ratio (4, 1).det (4, 6.0f).lvl (5, 0.15f).ratio (5, 1).lvl (6, 0.1f).ratio (6, 2)
            .allEnv (0.2f, 2.0f, 0.85f, 1.6f).uni (4, 10.0f, 0.8f),

        // ================================================================ Warm
        warm ("Warm Strings")
            .algo (6).mode (1, OpModeWave).scan (1, Saw)
            .lvl (1, 0.8f).lvl (2, 0.4f).ratio (2, 1).fb (2, 0.35f)
            .lvl (3, 0.5f).ratio (3, 1).det (3, 8.0f).lvl (4, 0.35f).ratio (4, 1).fb (4, 0.4f)
            .lvl (5, 0.4f).ratio (5, 2).det (5, -8.0f).lvl (6, 0.25f).ratio (6, 2)
            .uni (5, 14.0f, 0.9f).allEnv (0.35f, 2.0f, 0.85f, 1.2f)
            .lfo (1, LfoSine, 5.5f).slot (1, SrcLfo1, DstPitch, 0.003f),

        warm ("Warm Brass")                                 // индекс следует за огибающей — как у меди
            .algo (3).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.5f).ratio (2, 1).fb (2, 0.3f).env (2, 0.06f, 0.6f, 0.6f, 0.3f)
            .lvl (3, 0.5f).ratio (3, 1).lvl (4, 0.4f).ratio (4, 1).env (4, 0.08f, 0.5f, 0.55f, 0.3f)
            .allEnv (0.05f, 0.8f, 0.8f, 0.35f).menv (0.06f, 0.5f, 0.6f, 0.3f).slot (1, SrcModEnv, DstFMAmount, 0.3f)
            .filt (FiltLP24, 1800.0f, 0.2f, 0.45f, 0.5f, 0.2f),

        warm ("Warm Analog")
            .algo (4).mode (1, OpModeWave).scan (1, Saw)
            .lvl (1, 0.8f).lvl (2, 0.3f).ratio (2, 1).fb (2, 0.5f).lvl (3, 0.2f).ratio (3, 1)
            .lvl (4, 0.6f).ratio (4, 1).det (4, 9.0f).lvl (5, 0.35f).ratio (5, 1).fb (5, 0.55f).lvl (6, 0.2f).ratio (6, 2)
            .uni (4, 12.0f, 0.8f).filt (FiltLP24, 2400.0f, 0.3f, 0.4f, 0.5f, 0.2f),

        warm ("Warm Organ")
            .algo (8).mode (1, OpModeWave).scan (1, SineOct)
            .lvl (1, 0.75f).lvl (2, 0.6f).ratio (2, 2).lvl (3, 0.5f).ratio (3, 3).lvl (4, 0.45f).ratio (4, 4)
            .lvl (5, 0.4f).ratio (5, 0).lvl (6, 0.2f).ratio (6, 1)
            .allEnv (0.01f, 0.5f, 1.0f, 0.15f).uni (2, 5.0f, 0.5f).filt (FiltLP12, 5000.0f, 0.1f, 0.0f, 0.2f, 0.25f)
            .lfo (1, LfoSine, 6.2f).slot (1, SrcLfo1, DstPitch, 0.002f),

        warm ("Warm Pad")
            .algo (6).mode (1, OpModeGrain).pos (1, 0.35f).grain (1, 0.2f, 3.5f, 0.25f)
            .lvl (1, 0.8f).lvl (2, 0.3f).ratio (2, 1).fb (2, 0.3f)
            .lvl (3, 0.45f).ratio (3, 1).det (3, -7.0f).lvl (4, 0.35f).ratio (4, 1).fb (4, 0.35f)
            .lvl (5, 0.35f).ratio (5, 2).det (5, 7.0f).lvl (6, 0.2f).ratio (6, 1)
            .uni (4, 12.0f, 0.9f).allEnv (1.0f, 3.0f, 0.85f, 2.8f),

        warm ("Warm Bass")
            .algo (3).mode (1, OpModeWave).scan (1, Saw).mono (true, 0.04f)
            .lvl (1, 0.85f).lvl (2, 0.45f).ratio (2, 1).fb (2, 0.4f).env (2, 0.003f, 0.5f, 0.4f, 0.2f)
            .lvl (3, 0.6f).ratio (3, 0).lvl (4, 0.25f).ratio (4, 1)
            .allEnv (0.004f, 0.8f, 0.85f, 0.15f).uni (1, 0.0f, 0.0f)
            .filt (FiltLP24, 900.0f, 0.25f, 0.5f, 0.6f, 0.25f).menv (0.003f, 0.35f, 0.15f, 0.2f),

        warm ("Warm Lead")
            .algo (1).mode (1, OpModeSample).mono (true, 0.07f)
            .lvl (1, 0.85f).lvl (2, 0.45f).ratio (2, 1).lvl (3, 0.3f).ratio (3, 2).lvl (4, 0.2f).ratio (4, 1).fb (4, 0.4f)
            .allEnv (0.02f, 1.0f, 0.85f, 0.3f).uni (2, 8.0f, 0.5f)
            .lfo (1, LfoSine, 5.5f).slot (1, SrcLfo1, DstPitch, 0.004f).slot (2, SrcModWheel, DstFMAmount, 0.4f)
            .filt (FiltLP24, 3000.0f, 0.25f, 0.2f, 0.5f, 0.2f),

        warm ("Warm Tape")
            .algo (6).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.25f).ratio (2, 1).fb (2, 0.25f).lvl (3, 0.4f).ratio (3, 1).lvl (4, 0.25f).ratio (4, 2)
            .allEnv (0.02f, 2.0f, 0.8f, 0.9f).filt (FiltLP12, 3500.0f, 0.1f, 0.0f, 0.4f, 0.35f)
            .lfo (2, LfoSmoothRandom, 0.6f).slot (1, SrcLfo2, DstPitch, 0.003f),   // «плавание» ленты

        warm ("Warm Ember")
            .algo (12).mode (1, OpModeWave).scan (1, Saw)
            .lvl (1, 0.8f).lvl (2, 0.35f).ratio (2, 1).fb (2, 0.45f).lvl (3, 0.2f).ratio (3, 2).lvl (4, 0.15f).ratio (4, 1)
            .allEnv (0.6f, 3.0f, 0.8f, 2.5f).uni (3, 10.0f, 0.8f)
            .lfo (2, LfoSmoothRandom, 0.3f).slot (2, SrcLfo2, DstCutoff, 0.15f),

        warm ("Warm Honey")
            .algo (21).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.35f).ratio (2, 1).lvl (3, 0.2f).ratio (3, 2)
            .lvl (4, 0.5f).ratio (4, 1).det (4, -6.0f).lvl (5, 0.25f).ratio (5, 1).fb (5, 0.3f).lvl (6, 0.2f).ratio (6, 2)
            .allEnv (0.08f, 1.8f, 0.8f, 1.0f).filt (FiltLP24, 2000.0f, 0.2f, 0.3f, 0.5f, 0.15f),

        // ================================================================ Hollow
        hollow ("Hollow Reed")                              // 1:2 — нечётные гармоники
            .algo (6).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.45f).ratio (2, 2).env (2, 0.05f, 0.6f, 0.7f, 0.3f)
            .lvl (3, 0.3f).ratio (3, 1).lvl (4, 0.3f).ratio (4, 2)
            .lfo (1, LfoSine, 5.0f).slot (1, SrcLfo1, DstPitch, 0.003f),

        hollow ("Hollow Pipe")
            .algo (2).mode (1, OpModeWave).scan (1, Sq)
            .lvl (1, 0.85f).lvl (2, 0.4f).ratio (2, 1).lvl (3, 0.3f).ratio (3, 2).lvl (4, 0.08f).ratio (4, 12).fb (4, 0.8f)
            .allEnv (0.08f, 1.0f, 0.85f, 0.5f).filt (FiltBP, 1800.0f, 0.12f, 0.0f, 0.7f),

        hollow ("Hollow Clarinet")
            .algo (6).mode (1, OpModeWave).scan (1, Sq)
            .lvl (1, 0.85f).lvl (2, 0.55f).ratio (2, 2).env (2, 0.06f, 0.8f, 0.6f, 0.3f)
            .allEnv (0.04f, 0.8f, 0.85f, 0.3f).filt (FiltLP12, 3000.0f, 0.1f, 0.0f, 0.6f)
            .slot (1, SrcVelocity, OpLvl (2), 0.2f),

        hollow ("Hollow Wood")
            .algo (3).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.4f).ratio (2, 4).env (2, 0.002f, 0.3f, 0.0f, 0.2f)
            .lvl (3, 0.35f).ratio (3, 2).lvl (4, 0.25f).ratio (4, 4)
            .allEnv (0.003f, 1.2f, 0.0f, 0.5f).filt (FiltBP, 1100.0f, 0.2f, 0.2f, 0.7f),

        hollow ("Hollow Box")
            .algo (12).mode (1, OpModeWave).scan (1, Pulse25)
            .lvl (1, 0.85f).lvl (2, 0.4f).ratio (2, 2).lvl (3, 0.2f).ratio (3, 4).lvl (4, 0.15f).ratio (4, 6)
            .filt (FiltNotch, 1500.0f, 0.3f, 0.0f, 0.6f),

        hollow ("Hollow Vox")
            .algo (6).mode (1, OpModeWave).scan (1, Formant)
            .lvl (1, 0.85f).lvl (2, 0.3f).ratio (2, 2).lvl (3, 0.35f).ratio (3, 3).lvl (4, 0.2f).ratio (4, 2)
            .allEnv (0.2f, 1.5f, 0.85f, 0.8f).filt (FiltBP, 900.0f, 0.3f, 0.0f, 0.7f)
            .lfo (1, LfoSine, 5.0f).slot (1, SrcLfo1, DstPitch, 0.004f),

        hollow ("Hollow Tube")
            .algo (1).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.35f).ratio (2, 2).lvl (3, 0.2f).ratio (3, 2).lvl (4, 0.1f).ratio (4, 4)
            .allEnv (0.1f, 1.0f, 0.8f, 0.6f).uni (2, 6.0f, 0.5f),

        hollow ("Hollow Square")
            .algo (6).mode (1, OpModeWave).scan (1, Sq)
            .lvl (1, 0.85f).lvl (2, 0.5f).ratio (2, 2).lvl (3, 0.4f).ratio (3, 0).lvl (4, 0.3f).ratio (4, 2)
            .allEnv (0.005f, 0.8f, 0.8f, 0.3f).filt (FiltLP12, 4000.0f, 0.15f, 0.2f, 0.5f),

        hollow ("Hollow Ocarina")
            .algo (2).mode (1, OpModeWave).scan (1, Tri)
            .lvl (1, 0.85f).lvl (2, 0.2f).ratio (2, 2).lvl (3, 0.08f).ratio (3, 11).fb (3, 0.75f)
            .allEnv (0.12f, 1.0f, 0.85f, 0.4f).filt (FiltBP, 1600.0f, 0.15f, 0.0f, 0.8f)
            .lfo (1, LfoSine, 4.5f).slot (1, SrcLfo1, DstPitch, 0.005f),

        hollow ("Hollow Sub")                               // бывший Hollow Sub
            .algo (7).mono (true, 0.08f).mode (1, OpModeWave)
            .lvl (1, 0.85f).lvl (2, 0.4f).ratio (2, 0).scan (2, Tri)
            .lvl (3, 0.2f).ratio (3, 2).lvl (4, 0.4f)
            .lvl (5, 0.3f).ratio (5, 1).lvl (6, 0.25f).ratio (6, 1).fb (6, 0.3f)
            .allEnv (0.01f, 0.8f, 0.8f, 0.3f).set ("filterOn", 0)
            .slot (1, SrcVelocity, OpLvl (4), 0.3f),

        // ================================================================ Glassy
        glassy ("Glassy Tines")                             // зубец электропиано: модулятор ×14 с быстрым спадом
            .algo (6).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.35f).ratio (2, 14).env (2, 0.002f, 0.4f, 0.0f, 0.2f).vel (2, 0.9f)
            .lvl (3, 0.45f).ratio (3, 1).lvl (4, 0.3f).ratio (4, 1).env (4, 0.002f, 1.5f, 0.1f, 0.6f),

        glassy ("Glassy Keys")
            .algo (3).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.3f).ratio (2, 7).env (2, 0.002f, 0.6f, 0.05f, 0.3f)
            .lvl (3, 0.4f).ratio (3, 2).lvl (4, 0.25f).ratio (4, 1).lvl (5, 0.15f).ratio (5, 14)
            .env (5, 0.002f, 0.3f, 0.0f, 0.2f),

        glassy ("Glassy Harp")
            .algo (6).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.3f).ratio (2, 4).env (2, 0.001f, 0.35f, 0.0f, 0.2f)
            .lvl (3, 0.35f).ratio (3, 2).lvl (4, 0.15f).ratio (4, 8).env (4, 0.001f, 0.2f, 0.0f, 0.2f)
            .allEnv (0.002f, 2.2f, 0.0f, 2.0f).uni (2, 4.0f, 0.7f),

        glassy ("Glassy Celesta")
            .algo (12).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.25f).ratio (2, 4).lvl (3, 0.2f).ratio (3, 9).lvl (4, 0.12f).ratio (4, 14)
            .env (2, 0.001f, 0.5f, 0.0f, 0.3f).env (3, 0.001f, 0.3f, 0.0f, 0.2f).env (4, 0.001f, 0.2f, 0.0f, 0.2f)
            .allEnv (0.002f, 1.8f, 0.0f, 1.5f),

        glassy ("Glassy Shards")
            .algo (10).mode (1, OpModeWave).scan (1, Sine)
            .lvl (1, 0.85f).lvl (2, 0.3f).ratio (2, 11).lvl (3, 0.25f).ratio (3, 7, 0.01f).lvl (4, 0.15f).ratio (4, 17)
            .lvl (5, 0.1f).ratio (5, 3).env (2, 0.001f, 0.8f, 0.0f, 0.4f).env (3, 0.001f, 1.2f, 0.0f, 0.5f)
            .filt (FiltHP, 400.0f, 0.1f),

        glassy ("Glassy Pluck")
            .algo (3).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.4f).ratio (2, 7).env (2, 0.001f, 0.18f, 0.0f, 0.1f)
            .lvl (3, 0.35f).ratio (3, 1).lvl (4, 0.2f).ratio (4, 3)
            .allEnv (0.002f, 0.9f, 0.0f, 0.4f).allCurve (0.0f, 0.75f, 0.6f),

        glassy ("Glassy Mallet")
            .algo (6).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.35f).ratio (2, 10).env (2, 0.001f, 0.12f, 0.0f, 0.1f)
            .lvl (3, 0.35f).ratio (3, 4).lvl (4, 0.2f).ratio (4, 1)
            .allEnv (0.002f, 1.4f, 0.0f, 0.8f),

        glassy ("Glassy Pad")                               // бывший Glass Pad
            .algo (6).uni (4, 14.0f, 0.8f).filt (FiltLP12, 6500.0f, 0.15f).mode (1, OpModeWave)
            .lvl (1, 0.75f).lvl (3, 0.6f).lvl (5, 0.5f)
            .ratio (3, 2).ratio (5, 4).det (5, 6.0f)
            .lvl (2, 0.38f).ratio (2, 3).lvl (4, 0.3f).ratio (4, 7).lvl (6, 0.25f).ratio (6, 1, 0.5f)
            .allEnv (2.2f, 3.0f, 0.75f, 4.5f).lfo (1, LfoSine, 0.12f)
            .slot (1, SrcLfo1, DstFMAmount, 0.25f).slot (2, SrcModWheel, OpLvl (2), 0.4f),

        glassy ("Glassy Rain")
            .algo (6).mode (1, OpModeGrain).pos (1, 0.4f).grain (1, 0.05f, 5.0f, 0.7f)
            .lvl (1, 0.85f).lvl (2, 0.3f).ratio (2, 9).env (2, 0.001f, 0.3f, 0.1f, 0.4f)
            .lvl (3, 0.3f).ratio (3, 4).lvl (4, 0.15f).ratio (4, 11)
            .allEnv (0.01f, 2.5f, 0.3f, 2.5f).uni (3, 10.0f, 1.0f)
            .lfo (2, LfoSampleHold, 7.0f).slot (2, SrcLfo2, DstPan, 0.4f),

        glassy ("Glassy Chime")
            .algo (12).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.3f).ratio (2, 3).lvl (3, 0.2f).ratio (3, 7).lvl (4, 0.15f).ratio (4, 14)
            .env (3, 0.001f, 0.6f, 0.0f, 0.4f).env (4, 0.001f, 0.25f, 0.0f, 0.2f)
            .allEnv (0.002f, 4.0f, 0.0f, 3.0f),

        // ================================================================ Bright
        bright ("Bright Lead")
            .algo (1).mode (1, OpModeSample).mono (true, 0.05f)
            .lvl (1, 0.85f).lvl (2, 0.6f).ratio (2, 1).lvl (3, 0.5f).ratio (3, 2).lvl (4, 0.3f).ratio (4, 1).fb (4, 0.5f)
            .lfo (1, LfoSine, 5.5f).slot (1, SrcLfo1, DstPitch, 0.004f).slot (2, SrcModWheel, DstFMAmount, 0.5f),

        bright ("Bright Stab")
            .algo (6).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.65f).ratio (2, 1).fb (2, 0.4f).env (2, 0.002f, 0.4f, 0.3f, 0.2f)
            .lvl (3, 0.6f).ratio (3, 1).det (3, 8.0f).lvl (4, 0.5f).ratio (4, 2)
            .allEnv (0.002f, 0.5f, 0.0f, 0.25f).uni (4, 16.0f, 0.9f),

        bright ("Bright Saw")
            .algo (6).mode (1, OpModeWave).scan (1, Saw)
            .lvl (1, 0.8f).lvl (2, 0.5f).ratio (2, 1).fb (2, 0.7f)
            .lvl (3, 0.6f).ratio (3, 1).det (3, 10.0f).lvl (4, 0.5f).ratio (4, 1).fb (4, 0.7f)
            .uni (6, 20.0f, 1.0f),

        bright ("Bright Brass")
            .algo (3).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.7f).ratio (2, 1).fb (2, 0.35f).env (2, 0.03f, 0.4f, 0.75f, 0.2f)
            .lvl (3, 0.6f).ratio (3, 1).lvl (4, 0.55f).ratio (4, 1).env (4, 0.04f, 0.4f, 0.7f, 0.2f)
            .allEnv (0.025f, 0.6f, 0.85f, 0.25f).menv (0.03f, 0.4f, 0.7f, 0.2f).slot (2, SrcModEnv, DstFMAmount, 0.35f),

        bright ("Bright Pluck")
            .algo (6).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.6f).ratio (2, 2).env (2, 0.001f, 0.25f, 0.0f, 0.15f)
            .lvl (3, 0.5f).ratio (3, 1).lvl (4, 0.5f).ratio (4, 3).env (4, 0.001f, 0.2f, 0.0f, 0.15f)
            .allEnv (0.002f, 0.6f, 0.0f, 0.3f).allCurve (0.0f, 0.7f, 0.6f),

        bright ("Bright Sync")
            .algo (1).mode (1, OpModeWave).scan (1, Saw)
            .lvl (1, 0.85f).lvl (2, 0.7f).ratio (2, 3).lvl (3, 0.3f).ratio (3, 1)
            .lfo (2, LfoTriangle, 0.4f).slot (2, SrcLfo2, OpLvl (2), 0.25f).slot (3, SrcModEnv, OpPitch (2), 0.3f)
            .menv (0.002f, 0.6f, 0.0f, 0.3f),

        bright ("Bright Chord")
            .algo (8).mode (1, OpModeSample)
            .lvl (1, 0.8f).lvl (2, 0.6f).ratio (2, 1).det (2, 5.0f).lvl (3, 0.55f).ratio (3, 2)
            .lvl (4, 0.5f).ratio (4, 3).lvl (5, 0.5f).ratio (5, 1).det (5, -5.0f).lvl (6, 0.5f).ratio (6, 5)
            .allEnv (0.003f, 0.8f, 0.3f, 0.5f),

        bright ("Bright Supersaw")
            .algo (4).mode (1, OpModeWave).scan (1, Saw)
            .lvl (1, 0.8f).lvl (2, 0.5f).ratio (2, 1).fb (2, 0.8f).lvl (3, 0.3f).ratio (3, 1)
            .lvl (4, 0.7f).ratio (4, 1).lvl (5, 0.5f).ratio (5, 1).fb (5, 0.8f).lvl (6, 0.2f).ratio (6, 2)
            .uni (8, 30.0f, 1.0f).allEnv (0.01f, 1.0f, 0.9f, 0.6f),

        bright ("Bright Arp")
            .algo (6).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.55f).ratio (2, 1).env (2, 0.001f, 0.15f, 0.2f, 0.1f)
            .lvl (3, 0.45f).ratio (3, 2).allEnv (0.001f, 0.35f, 0.0f, 0.15f)
            .lfoSync (1, LfoSampleHold, 10).slot (1, SrcLfo1, OpLvl (2), 0.25f),

        bright ("Bright Spark")
            .algo (10).mode (1, OpModeWave).scan (1, Pulse10)
            .lvl (1, 0.85f).lvl (2, 0.4f).ratio (2, 1).lvl (3, 0.35f).ratio (3, 2).lvl (4, 0.3f).ratio (4, 3)
            .lvl (5, 0.25f).ratio (5, 5).lvl (6, 0.2f).ratio (6, 8).env (6, 0.001f, 0.15f, 0.0f, 0.1f),

        // ================================================================ Hard
        hard ("Hard Bass")
            .algo (3).mode (1, OpModeSample).mono (true, 0.03f)
            .lvl (1, 0.85f).lvl (2, 0.7f).ratio (2, 1).fb (2, 0.6f).env (2, 0.001f, 0.3f, 0.4f, 0.1f)
            .lvl (3, 0.7f).ratio (3, 0).lvl (4, 0.4f).ratio (4, 1)
            .filt (FiltLP24, 1500.0f, 0.35f, 0.5f, 0.4f, 0.5f),

        hard ("Hard Reese")
            .algo (4).mode (1, OpModeWave).scan (1, Saw).mono (true, 0.06f)
            .lvl (1, 0.8f).lvl (2, 0.55f).ratio (2, 1).fb (2, 0.7f).lvl (3, 0.2f).ratio (3, 1)
            .lvl (4, 0.75f).ratio (4, 1).det (4, 18.0f).lvl (5, 0.55f).ratio (5, 1).fb (5, 0.7f)
            .uni (3, 25.0f, 0.6f).filt (FiltLP24, 1800.0f, 0.3f, 0.2f, 0.3f, 0.6f)
            .lfo (2, LfoSine, 0.3f).slot (2, SrcLfo2, DstCutoff, 0.2f),

        hard ("Hard Stab")
            .algo (6).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.75f).ratio (2, 1).fb (2, 0.5f).lvl (3, 0.7f).ratio (3, 1).det (3, 12.0f)
            .lvl (4, 0.6f).ratio (4, 3).allEnv (0.001f, 0.35f, 0.0f, 0.2f).uni (4, 18.0f, 0.9f),

        hard ("Hard Growl")
            .algo (1).mode (1, OpModeSample).mono (true, 0.05f)
            .lvl (1, 0.85f).lvl (2, 0.7f).ratio (2, 1).lvl (3, 0.55f).ratio (3, 2).fb (3, 0.5f).lvl (4, 0.4f).ratio (4, 1)
            .lfo (1, LfoTriangle, 3.0f).slot (1, SrcLfo1, OpLvl (2), 0.25f).slot (2, SrcLfo1, DstCutoff, 0.3f),

        hard ("Hard Lead")
            .algo (1).mode (1, OpModeWave).scan (1, Saw).mono (true, 0.04f)
            .lvl (1, 0.85f).lvl (2, 0.7f).ratio (2, 2).fb (2, 0.4f).lvl (3, 0.5f).ratio (3, 1)
            .lfo (1, LfoSine, 5.8f).slot (1, SrcLfo1, DstPitch, 0.004f).slot (2, SrcModWheel, DstFMAmount, 0.6f),

        hard ("Hard Hit")
            .algo (10).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.7f).ratio (2, 1).fb (2, 0.8f).lvl (3, 0.6f).ratio (3, 3)
            .lvl (4, 0.4f).ratio (4, 7).allEnv (0.001f, 0.25f, 0.0f, 0.15f).allCurve (0.0f, 0.8f, 0.7f)
            .slot (1, SrcModEnv, OpPitch (1), 0.15f).menv (0.001f, 0.08f, 0.0f, 0.05f),

        hard ("Hard Distort")
            .algo (6).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.8f).ratio (2, 1).fb (2, 0.9f).lvl (3, 0.5f).ratio (3, 1).lvl (4, 0.6f).ratio (4, 2)
            .filt (FiltLP24, 3500.0f, 0.35f, 0.3f, 0.3f, 0.8f),

        hard ("Hard Grit")
            .algo (12).mode (1, OpModeGrain).pos (1, 0.2f).grain (1, 0.04f, 5.0f, 0.4f)
            .lvl (1, 0.85f).lvl (2, 0.6f).ratio (2, 1).fb (2, 0.9f).lvl (3, 0.5f).ratio (3, 5).lvl (4, 0.4f).ratio (4, 2)
            .filt (FiltHP, 300.0f, 0.2f, 0.0f, 0.2f, 0.6f),

        hard ("Hard Chaos")                                 // бывший Chaos Engine
            .algo (32).fm (1.4f).mono (false, 0.25f).mode (1, OpModeWave)
            .filt (FiltLP24, 3000.0f, 0.45f, 0.4f, 0.0f, 0.4f).menv (0.01f, 1.2f, 0.2f, 0.5f)
            .lvl (1, 0.8f).lvl (4, 0.7f)
            .lvl (2, 0.4f).ratio (2, 1).lvl (3, 0.35f).ratio (3, 2)
            .lvl (5, 0.4f).ratio (5, 3).lvl (6, 0.3f).ratio (6, 0).fb (6, 0.6f)
            .allEnv (0.01f, 1.0f, 0.7f, 0.6f).allCurve (0.0f, 0.5f, 0.5f)
            .lfoSync (1, LfoSampleHold, 10)
            .slot (1, SrcLfo1, OpLvl (5), 0.35f).slot (2, SrcModEnv, DstFMAmount, 0.5f).slot (8, SrcNone, DstNone, 0.0f),

        hard ("Hard Punch")
            .algo (3).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.65f).ratio (2, 1).env (2, 0.001f, 0.12f, 0.2f, 0.1f)
            .lvl (3, 0.75f).ratio (3, 0).lvl (4, 0.5f).ratio (4, 1).env (4, 0.001f, 0.1f, 0.0f, 0.1f)
            .allEnv (0.001f, 0.5f, 0.6f, 0.2f).slot (1, SrcModEnv, DstPitch, 0.02f).menv (0.001f, 0.06f, 0.0f, 0.05f),

        // ================================================================ Metallic
        metallic ("Metallic Bell")                          // бывший Metal Bell
            .algo (10).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.4f).ratio (2, 3, 0.17f)
            .lvl (3, 0.3f).ratio (3, 7).det (3, 11.0f).lvl (4, 0.22f).fixedHz (4, 1870.0f)
            .lvl (5, 0.18f).ratio (5, 11).allEnv (0.002f, 6.0f, 0.0f, 3.0f)
            .env (3, 0.001f, 0.8f, 0.0f, 0.5f).env (4, 0.001f, 0.25f, 0.0f, 0.3f),

        metallic ("Metallic Gong")
            .algo (1).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.5f).ratio (2, 1, 0.41f).lvl (3, 0.4f).ratio (3, 2, 0.37f).lvl (4, 0.3f).fixedHz (4, 230.0f)
            .allEnv (0.01f, 8.0f, 0.0f, 6.0f).env (2, 0.4f, 6.0f, 0.2f, 5.0f)
            .uni (2, 6.0f, 0.8f),

        metallic ("Metallic Tine")
            .algo (6).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.4f).ratio (2, 5, 0.2f).env (2, 0.001f, 0.6f, 0.0f, 0.3f)
            .lvl (3, 0.35f).ratio (3, 1).lvl (4, 0.3f).ratio (4, 3, 0.45f).allEnv (0.002f, 3.0f, 0.0f, 2.0f),

        metallic ("Metallic Plate")
            .algo (12).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.35f).ratio (2, 2, 0.31f).lvl (3, 0.3f).ratio (3, 4, 0.13f)
            .lvl (4, 0.25f).fixedHz (4, 3150.0f).allEnv (0.002f, 4.0f, 0.0f, 3.0f)
            .filt (FiltHP, 250.0f, 0.1f),

        metallic ("Metallic Clang")
            .algo (10).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.6f).ratio (2, 1, 0.41f).fb (2, 0.4f).lvl (3, 0.5f).ratio (3, 3, 0.5f)
            .lvl (4, 0.4f).fixedHz (4, 777.0f).lvl (5, 0.3f).ratio (5, 9, 0.11f)
            .allEnv (0.001f, 1.5f, 0.0f, 1.0f),

        metallic ("Metallic Bowl")                          // поющая чаша: медленные биения
            .algo (6).mode (1, OpModeWave).scan (1, Sine)
            .lvl (1, 0.85f).lvl (2, 0.2f).ratio (2, 2, 0.27f)
            .lvl (3, 0.5f).ratio (3, 2, 0.75f).det (3, 3.0f).lvl (4, 0.15f).ratio (4, 5, 0.1f)
            .lvl (5, 0.4f).ratio (5, 5, 0.37f).allEnv (0.05f, 9.0f, 0.0f, 8.0f).uni (2, 3.0f, 0.9f),

        metallic ("Metallic Ring")                          // кольцевая модуляция через Ring 6
            .algo (24).fm (0.7f).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.4f).ratio (2, 1, 0.5f).lvl (3, 0.3f).ratio (3, 3, 0.3f)
            .lvl (4, 0.2f).ratio (4, 7, 0.07f).lvl (5, 0.15f).ratio (5, 1).lvl (6, 0.15f).ratio (6, 2)
            .allEnv (0.005f, 3.0f, 0.3f, 2.0f),

        metallic ("Metallic Spring")
            .algo (3).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.5f).ratio (2, 1, 0.33f).env (2, 0.001f, 0.5f, 0.0f, 0.3f)
            .lvl (3, 0.4f).ratio (3, 1).lvl (4, 0.4f).ratio (4, 4, 0.21f)
            .allEnv (0.002f, 2.0f, 0.0f, 1.5f).slot (1, SrcModEnv, OpPitch (2), -0.08f).menv (0.001f, 0.6f, 0.0f, 0.4f),

        metallic ("Metallic Anvil")
            .algo (10).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.7f).fixedHz (2, 1220.0f).lvl (3, 0.5f).fixedHz (3, 2780.0f)
            .lvl (4, 0.35f).ratio (4, 1, 0.41f).allEnv (0.001f, 1.2f, 0.0f, 0.8f).allCurve (0.0f, 0.8f, 0.7f),

        metallic ("Metallic Wire")
            .algo (1).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.35f).ratio (2, 6, 0.03f).lvl (3, 0.25f).ratio (3, 1, 0.5f).fb (3, 0.3f)
            .allEnv (0.002f, 3.5f, 0.0f, 2.5f).uni (3, 7.0f, 0.9f),

        // ================================================================ Dark
        dark ("Dark Drone")
            .algo (6).mode (1, OpModeGrain).pos (1, 0.3f).grain (1, 0.35f, 3.0f, 0.3f)
            .lvl (1, 0.85f).lvl (2, 0.25f).ratio (2, 0).lvl (3, 0.5f).ratio (3, 0).lvl (4, 0.2f).ratio (4, 1)
            .lvl (5, 0.3f).ratio (5, 1).det (5, -12.0f).lvl (6, 0.15f).ratio (6, 1)
            .allEnv (3.0f, 6.0f, 1.0f, 6.0f).lfo (2, LfoSmoothRandom, 0.08f).slot (2, SrcLfo2, DstCutoff, 0.3f),

        dark ("Dark Sub")
            .algo (2).mode (1, OpModeWave).scan (1, Sine).mono (true, 0.05f)
            .lvl (1, 0.85f).lvl (2, 0.85f).ratio (2, 0).lvl (3, 0.2f).ratio (3, 1)
            .allEnv (0.01f, 1.0f, 0.9f, 0.3f).uni (1, 0.0f, 0.0f).filt (FiltLP24, 600.0f, 0.15f, 0.2f, 0.4f, 0.15f),

        dark ("Dark Pad")
            .algo (4).mode (1, OpModeWave).scan (1, Tri)
            .lvl (1, 0.8f).lvl (2, 0.3f).ratio (2, 1).lvl (3, 0.15f).ratio (3, 2)
            .lvl (4, 0.6f).ratio (4, 0).lvl (5, 0.3f).ratio (5, 1).lvl (6, 0.1f).ratio (6, 3)
            .uni (4, 14.0f, 0.9f).allEnv (1.5f, 4.0f, 0.9f, 4.0f),

        dark ("Dark Hum")
            .algo (6).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.3f).ratio (2, 1).fb (2, 0.3f).lvl (3, 0.45f).ratio (3, 0).lvl (4, 0.2f).ratio (4, 1)
            .allEnv (0.3f, 3.0f, 0.9f, 2.0f).filt (FiltLP24, 700.0f, 0.25f, 0.2f, 0.4f, 0.2f),

        dark ("Dark Cello")
            .algo (3).mode (1, OpModeSample).mono (true, 0.06f)
            .lvl (1, 0.85f).lvl (2, 0.45f).ratio (2, 1).fb (2, 0.45f).lvl (3, 0.4f).ratio (3, 1).lvl (4, 0.25f).ratio (4, 2)
            .allEnv (0.25f, 2.0f, 0.85f, 0.8f).filt (FiltLP24, 1300.0f, 0.2f, 0.25f, 0.5f, 0.2f)
            .lfo (1, LfoSine, 5.2f).slot (1, SrcLfo1, DstPitch, 0.004f),

        dark ("Dark Rumble")
            .algo (1).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.5f).ratio (2, 0).fb (2, 0.5f).lvl (3, 0.3f).ratio (3, 1).fb (3, 0.6f)
            .allEnv (0.5f, 4.0f, 0.9f, 3.0f).filt (FiltLP24, 450.0f, 0.3f, 0.2f, 0.3f, 0.35f)
            .lfo (2, LfoSmoothRandom, 0.5f).slot (2, SrcLfo2, DstFMAmount, 0.3f),

        dark ("Dark Fog")
            .algo (6).mode (1, OpModeGrain).pos (1, 0.6f).grain (1, 0.4f, 4.0f, 0.7f)
            .lvl (1, 0.85f).lvl (2, 0.15f).ratio (2, 1).lvl (3, 0.4f).ratio (3, 0).lvl (4, 0.08f).ratio (4, 9).fb (4, 0.8f)
            .uni (5, 18.0f, 1.0f).allEnv (2.5f, 5.0f, 0.9f, 6.0f).filt (FiltLP12, 1100.0f, 0.15f, 0.0f, 0.3f)
            .lfo (2, LfoSine, 0.04f).slot (2, SrcLfo2, OpScan (1), 0.3f),

        dark ("Dark Night")
            .algo (12).mode (1, OpModeWave).scan (1, SineOct)
            .lvl (1, 0.85f).lvl (2, 0.3f).ratio (2, 1).lvl (3, 0.2f).ratio (3, 0).lvl (4, 0.15f).ratio (4, 3)
            .allEnv (1.0f, 4.0f, 0.85f, 4.0f).filt (FiltLP24, 1000.0f, 0.3f, 0.3f, 0.4f),

        dark ("Dark Ring")                                  // бывший Dark Ring Drone
            .algo (24).uni (6, 22.0f, 1.0f).fm (0.8f).mode (1, OpModeWave)
            .filt (FiltLP24, 1800.0f, 0.3f, 0.0f, 0.0f, 0.0f)
            .lvl (1, 0.85f).lvl (2, 0.35f).ratio (2, 1).lvl (3, 0.3f).ratio (3, 1).det (3, -9.0f)
            .lvl (4, 0.28f).ratio (4, 2).scan (4, Saw).lvl (5, 0.22f).ratio (5, 3)
            .lvl (6, 0.3f).ratio (6, 0).fb (6, 0.4f)
            .allEnv (6.0f, 8.0f, 1.0f, 8.0f).allCurve (0.0f, 0.5f, 0.5f)
            .lfo (1, LfoSmoothRandom, 0.08f).lfo (2, LfoTriangle, 0.03f)
            .slot (1, SrcLfo1, DstFMAmount, 0.35f).slot (2, SrcLfo2, DstCutoff, 0.3f).slot (3, SrcLfo1, OpScan (4), 0.5f),

        dark ("Dark Undertow")
            .algo (21).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.25f).ratio (2, 1).lvl (3, 0.2f).ratio (3, 0)
            .lvl (4, 0.55f).ratio (4, 0).lvl (5, 0.3f).ratio (5, 1).fb (5, 0.35f).lvl (6, 0.15f).ratio (6, 1)
            .allEnv (0.8f, 4.0f, 0.9f, 3.5f).lfo (2, LfoSine, 0.12f).slot (2, SrcLfo2, OpLvl (2), 0.2f),

        // ================================================================ Moving
        moving ("Moving Waves")
            .algo (6).mode (1, OpModeGrain).pos (1, 0.2f).grain (1, 0.15f, 4.0f, 0.3f)
            .lvl (1, 0.85f).lvl (2, 0.3f).ratio (2, 1).lvl (3, 0.4f).ratio (3, 2).lvl (4, 0.2f).ratio (4, 1)
            .lfo (1, LfoSine, 0.2f).lfo (2, LfoTriangle, 0.13f)
            .slot (1, SrcLfo1, OpScan (1), 0.5f).slot (2, SrcLfo2, DstCutoff, 0.3f),

        moving ("Moving Pulse")
            .algo (6).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.4f).ratio (2, 1).lvl (3, 0.4f).ratio (3, 2).lvl (4, 0.3f).ratio (4, 1)
            .lfoSync (1, LfoSquare, 8).slot (1, SrcLfo1, DstVolume, -0.5f).slot (2, SrcLfo1, OpLvl (2), 0.2f),

        moving ("Moving Swirl")
            .algo (12).mode (1, OpModeWave).scan (1, Formant)
            .lvl (1, 0.85f).lvl (2, 0.35f).ratio (2, 2).lvl (3, 0.25f).ratio (3, 3).lvl (4, 0.2f).ratio (4, 5)
            .lfo (1, LfoSine, 0.3f).lfo (2, LfoSine, 0.17f)
            .slot (1, SrcLfo1, DstPan, 0.6f).slot (2, SrcLfo2, OpScan (1), 0.6f).slot (3, SrcLfo1, OpLvl (3), 0.2f),

        moving ("Moving Flicker")
            .algo (6).mode (1, OpModeGrain).pos (1, 0.5f).grain (1, 0.05f, 5.5f, 0.5f)
            .lvl (1, 0.85f).lvl (2, 0.3f).ratio (2, 3).lvl (3, 0.3f).ratio (3, 4).lvl (4, 0.2f).ratio (4, 1)
            .lfoSync (1, LfoSampleHold, 10).slot (1, SrcLfo1, OpScan (1), 0.4f).slot (2, SrcLfo1, DstCutoff, 0.25f),

        moving ("Moving Wobble")
            .algo (3).mode (1, OpModeSample).mono (true, 0.05f)
            .lvl (1, 0.85f).lvl (2, 0.5f).ratio (2, 1).fb (2, 0.5f).lvl (3, 0.5f).ratio (3, 0).lvl (4, 0.3f).ratio (4, 1)
            .filt (FiltLP24, 1500.0f, 0.35f, 0.0f, 0.3f, 0.3f)
            .lfoSync (1, LfoSine, 8).slot (1, SrcLfo1, DstCutoff, 0.45f).slot (2, SrcLfo1, OpLvl (2), 0.3f),

        moving ("Moving Gate")
            .algo (6).mode (1, OpModeSample)
            .lvl (1, 0.85f).lvl (2, 0.3f).ratio (2, 1).lvl (3, 0.45f).ratio (3, 2).lvl (4, 0.2f).ratio (4, 1)
            .allEnv (0.01f, 2.0f, 0.9f, 1.0f)
            .lfoSync (1, LfoSquare, 10).slot (1, SrcLfo1, DstVolume, -0.6f),

        moving ("Moving Shimmer")
            .algo (6).mode (1, OpModeGrain).pos (1, 0.35f).grain (1, 0.1f, 5.0f, 0.6f)
            .lvl (1, 0.85f).lvl (2, 0.15f).ratio (2, 2).lvl (3, 0.35f).ratio (3, 4).lvl (4, 0.1f).ratio (4, 7)
            .lvl (5, 0.25f).ratio (5, 8).lvl (6, 0.08f).ratio (6, 14)
            .uni (5, 18.0f, 1.0f).allEnv (1.0f, 3.0f, 0.85f, 4.0f).filt (FiltHP, 600.0f, 0.1f)
            .lfo (1, LfoSmoothRandom, 3.0f).slot (1, SrcLfo1, OpLvl (5), 0.2f).slot (2, SrcLfo1, DstPan, 0.4f),

        moving ("Moving Morph")
            .algo (10).mode (1, OpModeWave).scan (1, Sine)
            .lvl (1, 0.85f).lvl (2, 0.3f).ratio (2, 1).lvl (3, 0.25f).ratio (3, 2).lvl (4, 0.2f).ratio (4, 3)
            .lfo (1, LfoTriangle, 0.08f).lfo (2, LfoSine, 0.11f)
            .slot (1, SrcLfo1, OpScan (1), 1.0f).slot (2, SrcLfo2, DstFMAmount, 0.5f),

        moving ("Moving Orbit")
            .algo (8).mode (1, OpModeSample)
            .lvl (1, 0.8f).lvl (2, 0.4f).ratio (2, 1).det (2, 7.0f).pan (2, -0.7f).lvl (3, 0.4f).ratio (3, 2).pan (3, 0.7f)
            .lvl (4, 0.3f).ratio (4, 3).lvl (5, 0.3f).ratio (5, 1).lvl (6, 0.2f).ratio (6, 4)
            .allEnv (0.5f, 3.0f, 0.85f, 3.0f).lfo (1, LfoSine, 0.25f).slot (1, SrcLfo1, DstPan, 0.7f)
            .lfo (2, LfoSmoothRandom, 0.5f).slot (2, SrcLfo2, DstFMAmount, 0.4f),

        moving ("Moving Stutter")
            .algo (6).mode (1, OpModeGrain).pos (1, 0.15f).grain (1, 0.08f, 4.0f, 0.2f)
            .lvl (1, 0.85f).lvl (2, 0.35f).ratio (2, 2).lvl (3, 0.35f).ratio (3, 1).lvl (4, 0.2f).ratio (4, 3)
            .lfoSync (1, LfoSampleHold, 11).slot (1, SrcLfo1, OpScan (1), 0.6f)
            .lfoSync (2, LfoSquare, 10).slot (2, SrcLfo2, DstVolume, -0.4f),
    };
}
} // namespace mist
