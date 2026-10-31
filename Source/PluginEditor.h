#pragma once
#include "PluginProcessor.h"
#include "UI/LookAndFeel.h"
#include "UI/Widgets.h"
#include "UI/OperatorPanel.h"
#include "UI/RoutingPanel.h"
#include "UI/ModulationPanel.h"

namespace ferrite
{
// Верхняя панель: пресеты, глобальные параметры, осциллограф
class HeaderBar : public juce::Component
{
public:
    explicit HeaderBar (FerriteProcessor& p);
    void paint (juce::Graphics& g) override;
    void resized() override;
    void refreshPresetList();
    void tick();

    ScopeView scope;

private:
    void showPresetMenu();
    void savePreset();
    FerriteProcessor& proc;
    juce::TextButton presetButton, prev { "<" }, next { ">" }, save { "Save" }, init { "Init" };
    ParamChoice polyMode { "Voice" };
    ParamKnob glide { "Glide" }, bend { "Bend" }, uni { "Unison" }, uniDetune { "Detune" }, uniSpread { "Spread" },
              fm { "FM Amt" }, master { "Master" };
    juce::Label voicesLabel;
    std::unique_ptr<juce::AlertWindow> saveWindow;
};

class FerriteEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FerriteEditor (FerriteProcessor&);
    ~FerriteEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int kBaseW = 1280, kBaseH = 820;

private:
    void timerCallback() override;
    void selectOperator (int op);

    FerriteProcessor& proc;
    FerriteLookAndFeel lnf;

    // всё рисуется в базовом размере и масштабируется трансформом
    juce::Component content;
    HeaderBar header;
    std::vector<std::unique_ptr<OpCard>> cards;
    RoutingPanel routing;
    OperatorDetail detail;
    FilterPanel filter;
    LfoPanel lfo1, lfo2;
    ModMatrixPanel modMatrix;

    int lastSampleVersion = -1;
    std::vector<float> scopeTmp = std::vector<float> (8192);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FerriteEditor)
};
} // namespace ferrite
