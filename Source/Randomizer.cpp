#include "Randomizer.h"
#include "Algorithms.h"

namespace mist
{
juce::String RandomizerOptions::name (Group g)
{
    switch (g)
    {
        case Algorithm:     return "Algorithm";
        case Levels:        return "Levels & FM Amount";
        case Pitch:         return "Pitch (Coarse, Fine, Detune)";
        case Feedback:      return "Feedback";
        case WaveScan:      return "Wave Scan";
        case Envelopes:     return "Envelopes";
        case Filter:        return "Filter";
        case Unison:        return "Unison Detune";
        case Lfo:           return "LFO";
        case Grain:         return "Grain";
        case PanStereo:     return "Pan & Stereo Spread";
        case OperatorModes: return "Operator modes (Scrub / PM / Grain)";
        case NumGroups:     break;
    }
    return {};
}

juce::String RandomizerOptions::prefKey (Group g)
{
    static const char* keys[] = { "algorithm", "levels", "pitch", "feedback", "scan", "envelopes",
                                  "filter", "unison", "lfo", "grain", "pan", "modes" };
    return juce::String ("dice_") + keys[(int) g];
}

void randomizePatch (MistProcessor& p, juce::Random& rng, const RandomizerOptions& opt)
{
    using G = RandomizerOptions;
    auto set = [&p] (const juce::String& id, float v)
    {
        if (auto* prm = p.apvts.getParameter (id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 (v));
            prm->endChangeGesture();
        }
    };
    auto get = [&p] (const juce::String& id) { return p.apvts.getRawParameterValue (id)->load(); };
    auto uni = [&rng] (float a, float b) { return a + (b - a) * rng.nextFloat(); };
    auto logUni = [&rng] (float a, float b) { return a * std::pow (b / a, rng.nextFloat()); };

    // Все случайные числа тянем всегда, даже для запрещённых групп: тогда одна и та же
    // последовательность кубика не зависит от настроек (удобно для теста и предсказуемо).
    auto pick = [&] (G::Group g, const juce::String& id, float v) { if (opt.allows (g)) set (id, v); };

    // алгоритм 1..32 (Matrix не выбираем — там легко получить тишину)
    const int algo = 1 + rng.nextInt (kNumAlgorithms);
    pick (G::Algorithm, "algorithm", (float) algo);

    // кто несущий — по алгоритму, который реально будет звучать
    const int currentAlgo = (int) std::lround (get ("algorithm"));
    std::array<bool, kNumOps> carrier {};
    for (int i = 0; i < kNumOps; ++i)
        carrier[(size_t) i] = currentAlgo > 0 ? getAlgorithm (currentAlgo - 1).carrier[(size_t) i]
                                              : get (opId (i, "out")) > 0.0f;

    pick (G::Levels, "fmAmount", uni (0.3f, 1.4f));

    static const int coarseChoices[] = { 0, 1, 1, 1, 2, 2, 3, 4, 5, 7 };
    for (int i = 0; i < kNumOps; ++i)
    {
        const bool c = carrier[(size_t) i];
        if (opt.allows (G::Levels)) set (opId (i, "on"), 1.0f);
        if (opt.allows (G::Pitch))  set (opId (i, "fixed"), 0.0f);
        pick (G::Pitch, opId (i, "coarse"), (float) coarseChoices[rng.nextInt (10)]);
        { const bool keep = rng.nextInt (10) < 7; const float v = uni (-0.12f, 0.12f); pick (G::Pitch, opId (i, "fine"), keep ? 0.0f : v); }
        pick (G::Pitch, opId (i, "detune"), uni (-12.0f, 12.0f));
        { const float v = c ? uni (0.65f, 0.9f) : uni (0.15f, 0.6f); pick (G::Levels, opId (i, "level"), v); }
        const float fb = rng.nextFloat();
        pick (G::Feedback, opId (i, "feedback"), fb * fb * 0.5f);
        pick (G::WaveScan, opId (i, "scan"), rng.nextFloat());
        { const float v = uni (-0.3f, 0.3f); pick (G::PanStereo, opId (i, "pan"), c ? v : 0.0f); }

        if (opt.allows (G::Envelopes))
        {
            set (opId (i, "delay"), 0.0f);
            set (opId (i, "hold"), 0.0f);
        }
        pick (G::Envelopes, opId (i, "attack"), logUni (0.003f, c ? 2.5f : 3.0f));
        pick (G::Envelopes, opId (i, "decay"), logUni (0.3f, 6.0f));
        { const float v = c ? uni (0.5f, 1.0f) : uni (0.2f, 1.0f); pick (G::Envelopes, opId (i, "sustain"), v); }
        pick (G::Envelopes, opId (i, "release"), logUni (0.3f, 5.0f));
        // изгибы — умеренные, вокруг формы по умолчанию
        pick (G::Envelopes, opId (i, "aCurve"), uni (-0.4f, 0.6f));
        pick (G::Envelopes, opId (i, "dCurve"), uni (0.0f, 0.8f));
        pick (G::Envelopes, opId (i, "rCurve"), uni (0.1f, 0.8f));

        pick (G::Grain, opId (i, "grainSize"), logUni (0.04f, 0.4f));
        pick (G::Grain, opId (i, "grainDensity"), uni (2.0f, 5.0f));
        pick (G::Grain, opId (i, "grainSpray"), uni (0.0f, 0.5f));

        // режим: Scrub / Sample PM / Grain — только там, где есть семпл (без семпла режим ничего не меняет)
        const int mode = rng.nextInt (3);
        if (opt.allows (G::OperatorModes) && p.getSample (i) != nullptr)
            set (opId (i, "mode"), (float) mode);
    }

    // фильтр: включён/выключен и тип не трогаем, только безопасные значения
    pick (G::Filter, "cutoff", logUni (900.0f, 12000.0f));
    pick (G::Filter, "reso", uni (0.0f, 0.5f));
    pick (G::Filter, "filterEnv", uni (-0.2f, 0.5f));
    pick (G::Filter, "drive", uni (0.0f, 0.4f));

    pick (G::Unison, "uniDetune", uni (0.0f, 30.0f));
    pick (G::PanStereo, "uniSpread", uni (0.4f, 1.0f));

    pick (G::Envelopes, "menv_attack", logUni (0.005f, 1.0f));
    pick (G::Envelopes, "menv_decay", logUni (0.2f, 3.0f));
    pick (G::Envelopes, "menv_sustain", uni (0.0f, 0.6f));
    pick (G::Envelopes, "menv_release", logUni (0.2f, 3.0f));

    for (int l = 0; l < 2; ++l)
    {
        pick (G::Lfo, lfoId (l, "shape"), (float) rng.nextInt (7));
        pick (G::Lfo, lfoId (l, "rate"), logUni (0.05f, 6.0f));
    }

    p.presets.setCurrentName ("Random");
}
} // namespace mist
