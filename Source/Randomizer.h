#pragma once
#include "PluginProcessor.h"

namespace mist
{
// Какие группы параметров кубику можно менять. Настраивается в меню настроек → Randomizer,
// хранится в настройках плагина (не в пресете).
struct RandomizerOptions
{
    enum Group
    {
        Algorithm = 0,   // алгоритм
        Levels,          // FM Amount, уровни операторов
        Pitch,           // Coarse, Fine, Detune
        Feedback,
        WaveScan,
        Envelopes,       // огибающие операторов и Mod Env, изгибы
        Filter,          // срез, резонанс, Filter Env, Drive
        Unison,          // Uni Detune
        Lfo,
        Grain,           // Grain Size / Density / Spray
        PanStereo,       // Pan операторов, Uni Spread
        OperatorModes,   // Scrub / Sample PM / Grain — только у операторов с семплом
        NumGroups
    };

    std::array<bool, NumGroups> enabled { true, true, true, true, true, true, true, true, true, true, false, false };

    bool allows (Group g) const noexcept { return enabled[(size_t) g]; }
    static juce::String name (Group g);
    static juce::String prefKey (Group g);   // ключ в файле настроек
};

// Случайный патч. Меняются только «тембровые» параметры в безопасных диапазонах и только разрешённые группы.
// Никогда не трогаются: Master, Transpose, семплы и их start/loop/reverse/root/keytrack, Drone/Hold,
// оверсемплинг, интерполяция, полифония/глайд/бенд, Mod Matrix, число голосов унисона, имена слотов.
// Режимы операторов — только если включена группа OperatorModes.
// Несущие всегда слышны (уровень ≥ 65%, sustain ≥ 50%), срез фильтра ≥ 900 Гц, резонанс ≤ 50%.
void randomizePatch (MistProcessor& p, juce::Random& rng, const RandomizerOptions& options = {});
} // namespace mist
