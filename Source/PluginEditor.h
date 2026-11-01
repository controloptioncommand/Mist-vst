#pragma once
#include "PluginProcessor.h"
#include "UI/LookAndFeel.h"
#include "UI/Widgets.h"
#include "UI/Visualizer.h"
#include "UI/OperatorPanel.h"
#include "UI/RoutingPanel.h"
#include "UI/ModulationPanel.h"
#include "UI/About.h"
#include "Randomizer.h"

namespace mist
{
// Сегментированный переключатель
class Segmented : public juce::Component
{
public:
    struct Item { juce::String text; int value; };
    Segmented();
    void setItems (std::vector<Item> it) { items = std::move (it); repaint(); }
    void setSelectedValue (int v) { if (v != selected) { selected = v; repaint(); } }
    int getSelectedValue() const { return selected; }
    std::function<void (int)> onChange;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    bool keyPressed (const juce::KeyPress& k) override;
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { repaint(); }

private:
    void choose (int index);
    std::vector<Item> items;
    int selected = 0;
};

// Чип «Interpolation Hermite ⌄»: по клику — меню выбора
class PillChoice : public juce::Component
{
public:
    PillChoice (APVTS& s, const juce::String& paramId, const juce::String& caption);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent&) override { openMenu(); }
    bool keyPressed (const juce::KeyPress& k) override;
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { repaint(); }

private:
    void openMenu();
    juce::RangedAudioParameter* param;
    juce::String caption;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    int index = 0;
};

class OutMeter : public juce::Component
{
public:
    OutMeter() { setTitle ("Output level"); }
    void setLevel (float peak);
    void paint (juce::Graphics& g) override;

private:
    float level = 0.0f;
};

// Логотип Mist (Medeo Pixel) — открывает About
class LogoButton : public juce::Button
{
public:
    LogoButton() : juce::Button ("Mist") { setTitle ("About Mist"); }
    void paintButton (juce::Graphics& g, bool over, bool down) override;
};

class IconButton : public juce::Button
{
public:
    enum Kind { Settings, Prev, Next, Dice };
    IconButton (Kind k, const juce::String& title) : juce::Button (title), kind (k) { setTitle (title); }
    void paintButton (juce::Graphics& g, bool over, bool down) override;

private:
    Kind kind;
};

class MistEditor : public juce::AudioProcessorEditor, private juce::Timer, private juce::DarkModeSettingListener
{
public:
    explicit MistEditor (MistProcessor&);
    ~MistEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int kBaseW = 1120, kBaseH = 860;

    // для тестов
    void setThemeForTest (ThemeId id) { setThemeMode (id == ThemeId::Rubber ? ThemeMode::Rubber : ThemeMode::Oxygen); }
    void setView (int pageIndex, int vizMode, int op) { selectOperator (op); showPage (pageIndex); viz.setMode ((Visualizer::Mode) vizMode); }
    void showAboutForTest (bool show, bool game) { showAbout (show); if (show) about.showGame (game); }

private:
    void timerCallback() override;
    void darkModeSettingChanged() override;
    void selectOperator (int op);
    void showPage (int page);
    void showAbout (bool show);
    void showPresetMenu();
    void showSettingsMenu();
    void savePreset();
    void setThemeMode (ThemeMode m);
    void applyThemeMode();
    void setParam (const juce::String& id, float realValue);
    void updatePageTitle();
    void updateRoles();
    void layoutSlots();

    MistProcessor& proc;
    MistLookAndFeel lnf;
    ThemeMode themeMode = ThemeMode::System;
    juce::Component content;   // базовый размер, масштабируется трансформом

    // тулбар (стекло)
    LogoButton logo;
    IconButton prevPreset { IconButton::Prev, "Previous preset" }, nextPreset { IconButton::Next, "Next preset" },
               settingsButton { IconButton::Settings, "Settings" };
    juce::TextButton presetName;
    Segmented modeSwitch;
    OutMeter outMeter;
    juce::Rectangle<int> toolbarArea;

    Visualizer viz;
    std::vector<std::unique_ptr<SampleSlot>> slots;
    juce::Rectangle<int> slotsArea;
    int selectedOp = 0;
    int filledMask = -1;

    // панель оператора
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

    // подвал
    ParamToggle drone { "Drone" };
    IconButton dice { IconButton::Dice, "Randomize sound" };
    juce::Random rng;
    PillChoice interp, oversampling;
    juce::Label statusLabel;

    AboutPage about;
    std::unique_ptr<juce::AlertWindow> saveWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MistEditor)
};
} // namespace mist
