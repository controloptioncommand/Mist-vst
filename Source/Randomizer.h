#pragma once
#include "PluginProcessor.h"

namespace mist
{
// Случайный патч. Меняются только «тембровые» параметры в безопасных диапазонах.
// Не трогаются: Master, семплы и их start/loop/reverse/root/keytrack, режимы операторов,
// Drone, оверсемплинг, интерполяция, полифония/глайд/бенд, Mod Matrix, число голосов унисона.
// Несущие всегда слышны (уровень ≥ 65%, sustain ≥ 50%), срез фильтра ≥ 900 Гц, резонанс ≤ 50%.
void randomizePatch (MistProcessor& p, juce::Random& rng);
} // namespace mist
