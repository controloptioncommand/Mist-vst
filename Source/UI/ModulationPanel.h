#pragma once
#include "Widgets.h"
#include "../PluginProcessor.h"

namespace ferrite
{
class FilterPanel : public Section
{
public:
    explicit FilterPanel (FerriteProcessor& p);
    void resized() override;

private:
    ParamToggle on { "On" };
    ParamChoice type { "Type" };
    ParamKnob cutoff { "Cutoff" }, reso { "Reso" }, env { "Env Amt" }, key { "Key" }, drive { "Drive" };
    ParamKnob ma { "Attack" }, md { "Decay" }, ms { "Sustain" }, mr { "Release" };
    juce::Label modEnvLabel;
};

class LfoPanel : public Section
{
public:
    LfoPanel (FerriteProcessor& p, int index);
    void resized() override;
    void refresh();
    void paintOverChildren (juce::Graphics& g) override;

private:
    FerriteProcessor& proc;
    int index;
    ParamChoice shape { "Shape" }, div { "Division" };
    ParamToggle sync { "Sync" };
    ParamKnob rate { "Rate" };
    juce::Rectangle<int> meter;
    float shown = 0;
};

class ModMatrixPanel : public Section
{
public:
    explicit ModMatrixPanel (FerriteProcessor& p);
    void resized() override;

private:
    struct Row
    {
        juce::ComboBox src, dst;
        juce::Slider amt;
        std::unique_ptr<APVTS::ComboBoxAttachment> srcA, dstA;
        std::unique_ptr<APVTS::SliderAttachment> amtA;
    };
    std::array<Row, kNumModSlots> rows;
};
} // namespace ferrite
