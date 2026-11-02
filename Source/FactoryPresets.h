#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include <utility>

namespace mist
{
// Фактори-пресеты — шаблоны («формочки»): в Op 1 кладётся свой семпл, остальное задаёт характер.
// Загрузка шаблона не трогает семплы, их start/loop/reverse/root/keytrack, имена слотов и Transpose.
struct FactoryPreset
{
    juce::String name;       // «Airy Veil»: первое слово — характер
    juce::String category;   // характер: Airy, Soft, Warm, Hollow, Glassy, Bright, Hard, Metallic, Dark, Moving
    std::vector<std::pair<juce::String, float>> values;   // id -> значение в реальных единицах
};

constexpr int kTemplateSampleOp = 0;   // в шаблонах семпл — в Op 1 (во всех алгоритмах это несущая)

const std::vector<FactoryPreset>& getFactoryPresets();
juce::StringArray getTemplateCategories();   // в порядке меню
} // namespace mist
