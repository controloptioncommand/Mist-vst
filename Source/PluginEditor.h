#pragma once
#include "PluginProcessor.h"
#include "UI/LookAndFeel.h"
#include "UI/Widgets.h"
#include "UI/Visualizer.h"
#include "UI/OperatorPanel.h"
#include "UI/RoutingPanel.h"
#include "UI/ModulationPanel.h"

namespace mist
{
// Сегментированный переключатель (pill). Может быть привязан к choice-параметру.
class Segmented : public juce::Component
{
public:
    struct Item { juce::String text; int value; };
    void setItems (std::vector<Item> it) { items = std::move (it); repaint(); }
    void setSelectedValue (int v) { if (v != selected) { selected = v; repaint(); } }
    int getSelectedValue() const { return selected; }
    std::function<void (int)> onChange;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;

private:
    std::vector<Item> items;
    int selected = 0;
};

// Pill с подписью и значением, по клику — меню выбора (Interpolation Hermite ⌄)
class PillChoice : public juce::Component
{
public:
    PillChoice (APVTS& s, const juce::String& paramId, const juce::String& caption);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::RangedAudioParameter* param;
    juce::String caption;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    int index = 0;
};

class OutMeter : public juce::Component
{
public:
    void setLevel (float peak) { level = peak; repaint(); }
    void paint (juce::Graphics& g) override;

private:
    float level = 0.0f;
};

class MistEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MistEditor (MistProcessor&);
    ~MistEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    static constexpr int kBaseW = 1120, kBaseH = 860;
    void setThemeForTest (ThemeId id) { setTheme (id); }
    void setView (int pageIndex, int vizMode, int op) { selectOperator (op); showPage (pageIndex); viz.setMode ((Visualizer::Mode) vizMode); }

private:
    void timerCallback() override;
    void selectOperator (int op);
    void showPage (int page);
    void showPresetMenu();
    void showSettingsMenu();
    void savePreset();
    void setTheme (ThemeId id);
    static ThemeId loadSavedTheme();
    void setParam (const juce::String& id, float realValue);
    void updatePageTitle();

    MistProcessor& proc;
    MistLookAndFeel lnf;
    juce::Component content;   // рисуется в базовом размере, масштабируется трансформом

    // шапка
    juce::TextButton prevPreset { "<" }, nextPreset { ">" }, presetName, settingsButton;
    Segmented modeSwitch;
    OutMeter outMeter;
    juce::Rectangle<int> headerArea;

    Visualizer viz;
    std::vector<std::unique_ptr<SampleSlot>> slots;
    int selectedOp = 0;

    // настройки
    Segmented tabs;
    int page = 0;
    juce::Rectangle<int> settingsArea, titleArea, bottomArea;
    juce::String pageTitle, pageDescription;
    OperatorPage opPage;
    EnvelopePage envPage;
    RoutingPanel routing;
    VoicePage voicePage;
    FilterPanel filterPage;
    LfoPanel lfo1, lfo2;
    juce::Component lfoPage;
    ModMatrixPanel modPage;
    std::vector<juce::Component*> pages;

    // нижняя панель
    ParamToggle drone { "Drone" };
    PillChoice interp, oversampling;
    juce::Label cpuLabel;

    std::unique_ptr<juce::AlertWindow> saveWindow;
    std::vector<float> tmpL = std::vector<float> (MistProcessor::kScopeSize), tmpR = std::vector<float> (MistProcessor::kScopeSize);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MistEditor)
};
} // namespace mist
