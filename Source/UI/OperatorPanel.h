#pragma once
#include "Widgets.h"
#include "../PluginProcessor.h"

namespace loom
{
juce::String operatorRole (LoomProcessor& p, int op);   // "Carrier" / "Modulator" / "Unused" / "Off"
juce::String modeName (int mode);                       // "Scrub" / "Sample PM" / "Grain"
juce::String modeDescription (int mode);

// Слот семпла в левой колонке: имя, волна, start/length, reverse; drag & drop
class SampleSlot : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    SampleSlot (LoomProcessor& p, int op, std::function<void (int)> onSelect);
    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void setSelected (bool s) { if (s != selected) { selected = s; repaint(); } }
    void refresh();

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override;

    void openFileChooser();

private:
    float param (const char* name) const;
    LoomProcessor& proc;
    int op;
    std::function<void (int)> onSelect;
    bool selected = false, dragOver = false;
    juce::TextButton reverse { "Reverse" };
    std::unique_ptr<APVTS::ButtonAttachment> reverseAttachment;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> waveArea, powerArea;
    juce::String cachedRole;
};

// Большой вид волны выбранного оператора с маркерами start/loop
class WaveView : public juce::Component
{
public:
    explicit WaveView (LoomProcessor& p) : proc (p) {}
    void setOperator (int op) { opIndex = op; repaint(); }
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

private:
    float param (const char* name) const;
    juce::RangedAudioParameter* paramPtr (const char* name) const;
    LoomProcessor& proc;
    int opIndex = 0;
    juce::RangedAudioParameter* dragging = nullptr;
};

// Страница «Operator»: параметры выбранного оператора; набор второй строки зависит от режима
class OperatorPage : public juce::Component
{
public:
    explicit OperatorPage (LoomProcessor& p);
    void setOperator (int op);
    void resized() override;
    void refresh();

private:
    LoomProcessor& proc;
    int op = 0, shownMode = -1;
    juce::Colour col;

    ParamKnob level { "Depth" }, coarse { "Ratio" }, fine { "Fine" }, detune { "Detune" }, feedback { "Feedback" },
              velSens { "Velocity" }, pan { "Pan" };
    ParamKnob scan { "Position" }, start { "Start" }, loopStart { "Loop In" }, loopEnd { "Loop Out" }, xfade { "X-Fade" },
              root { "Root" }, grainSize { "Size" }, grainDensity { "Density" }, grainSpray { "Spray" },
              fixedHz { "Fixed Hz" }, out { "Matrix Out" };
    ParamToggle on { "On" }, fixed { "Fixed freq" }, keytrack { "Keytrack" };
    ParamChoice loopMode { "Loop" };
    WaveView wave;

    std::vector<ParamKnob*> allKnobs();
    std::vector<juce::Component*> secondRow();
};

class EnvelopePage : public juce::Component
{
public:
    explicit EnvelopePage (LoomProcessor& p);
    void setOperator (int op);
    void resized() override;
    void refresh();

private:
    LoomProcessor& proc;
    int op = 0;
    ParamKnob delay { "Delay" }, attack { "Attack" }, hold { "Hold" }, decay { "Decay" }, sustain { "Sustain" }, release { "Release" };
    EnvelopeView view;
};
} // namespace loom
