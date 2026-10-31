#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include <utility>

namespace loom
{
struct FactoryPreset
{
    juce::String name;
    std::vector<std::pair<juce::String, float>> values;   // id -> значение в реальных единицах
};

const std::vector<FactoryPreset>& getFactoryPresets();
} // namespace loom
