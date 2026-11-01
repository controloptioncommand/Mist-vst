#include "Randomizer.h"
#include "Algorithms.h"

namespace mist
{
void randomizePatch (MistProcessor& p, juce::Random& rng)
{
    auto set = [&p] (const juce::String& id, float v)
    {
        if (auto* prm = p.apvts.getParameter (id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 (v));
            prm->endChangeGesture();
        }
    };
    auto uni = [&rng] (float a, float b) { return a + (b - a) * rng.nextFloat(); };
    auto logUni = [&rng] (float a, float b) { return a * std::pow (b / a, rng.nextFloat()); };

    // алгоритм 1..32 (Matrix не выбираем — там легко получить тишину)
    const int algo = 1 + rng.nextInt (kNumAlgorithms);
    set ("algorithm", (float) algo);
    const auto& a = getAlgorithm (algo - 1);
    set ("fmAmount", uni (0.3f, 1.4f));

    static const int coarseChoices[] = { 0, 1, 1, 1, 2, 2, 3, 4, 5, 7 };
    for (int i = 0; i < kNumOps; ++i)
    {
        const bool carrier = a.carrier[(size_t) i];
        set (opId (i, "on"), 1.0f);
        set (opId (i, "fixed"), 0.0f);
        set (opId (i, "coarse"), (float) coarseChoices[rng.nextInt (10)]);
        set (opId (i, "fine"), rng.nextInt (10) < 7 ? 0.0f : uni (-0.12f, 0.12f));
        set (opId (i, "detune"), uni (-12.0f, 12.0f));
        set (opId (i, "level"), carrier ? uni (0.65f, 0.9f) : uni (0.15f, 0.6f));
        const float fb = rng.nextFloat();
        set (opId (i, "feedback"), fb * fb * 0.5f);
        set (opId (i, "scan"), rng.nextFloat());
        set (opId (i, "pan"), carrier ? uni (-0.3f, 0.3f) : 0.0f);

        set (opId (i, "delay"), 0.0f);
        set (opId (i, "hold"), 0.0f);
        set (opId (i, "attack"), logUni (0.003f, carrier ? 2.5f : 3.0f));
        set (opId (i, "decay"), logUni (0.3f, 6.0f));
        set (opId (i, "sustain"), carrier ? uni (0.5f, 1.0f) : uni (0.2f, 1.0f));
        set (opId (i, "release"), logUni (0.3f, 5.0f));

        set (opId (i, "grainSize"), logUni (0.04f, 0.4f));
        set (opId (i, "grainDensity"), uni (2.0f, 5.0f));
        set (opId (i, "grainSpray"), uni (0.0f, 0.5f));
    }

    // фильтр: включён/выключен и тип не трогаем, только безопасные значения
    set ("cutoff", logUni (900.0f, 12000.0f));
    set ("reso", uni (0.0f, 0.5f));
    set ("filterEnv", uni (-0.2f, 0.5f));
    set ("drive", uni (0.0f, 0.4f));

    set ("uniDetune", uni (0.0f, 30.0f));
    set ("uniSpread", uni (0.4f, 1.0f));

    set ("menv_attack", logUni (0.005f, 1.0f));
    set ("menv_decay", logUni (0.2f, 3.0f));
    set ("menv_sustain", uni (0.0f, 0.6f));
    set ("menv_release", logUni (0.2f, 3.0f));

    for (int l = 0; l < 2; ++l)
    {
        set (lfoId (l, "shape"), (float) rng.nextInt (7));
        set (lfoId (l, "rate"), logUni (0.05f, 6.0f));
    }

    p.presets.setCurrentName ("Random");
}
} // namespace mist
