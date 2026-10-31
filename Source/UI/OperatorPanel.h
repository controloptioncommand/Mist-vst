#pragma once
#include "Widgets.h"
#include "../PluginProcessor.h"

namespace ferrite
{
// Вид волны: семпл (с маркерами start/loop, их можно тянуть) или текущий кадр wavetable
class WaveView : public juce::Component
{
public:
    WaveView (FerriteProcessor& p) : proc (p) {}
    void setOperator (int op) { opIndex = op; repaint(); }
    void setCompact (bool c) { compact = c; }
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

private:
    float param (const char* name) const;
    juce::RangedAudioParameter* paramPtr (const char* name) const;
    FerriteProcessor& proc;
    int opIndex = 0;
    bool compact = false;
    juce::RangedAudioParameter* dragging = nullptr;
};

// Карточка оператора в верхнем ряду: выбор, on/off, уровень, перетаскивание семпла
class OpCard : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    OpCard (FerriteProcessor& p, int op, std::function<void (int)> onSelect);
    void paint (juce::Graphics& g) override;
    void paintOverChildren (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void setSelected (bool s) { selected = s; repaint(); }
    void refresh();

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    FerriteProcessor& proc;
    int op;
    std::function<void (int)> onSelect;
    bool selected = false, dragOver = false;
    ParamToggle onButton { "" };
    ParamKnob level;
    juce::Label ratioLabel, nameLabel;
    WaveView wave;
};

// Подробная панель выбранного оператора
class OperatorDetail : public Section, public juce::FileDragAndDropTarget
{
public:
    explicit OperatorDetail (FerriteProcessor& p);
    void setOperator (int op);
    int getOperator() const { return op; }
    void resized() override;
    void refresh();   // вызывается таймером

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    FerriteProcessor& proc;
    int op = 0;

    ParamChoice mode { "Mode" };
    ParamKnob coarse { "Ratio" }, fine { "Fine" }, detune { "Detune" }, fixedHz { "Fixed Hz" };
    ParamToggle fixed { "Fixed" };
    ParamKnob level { "Level" }, velSens { "Velocity" }, feedback { "Feedback" }, scan { "Scan" }, pan { "Pan" }, out { "Mtx Out" };
    ParamKnob delay { "Delay" }, attack { "Attack" }, hold { "Hold" }, decay { "Decay" }, sustain { "Sustain" }, release { "Release" };
    EnvelopeView envView;

    WaveView wave;
    ParamKnob start { "Start" }, loopStart { "Loop In" }, loopEnd { "Loop Out" }, xfade { "X-Fade" }, root { "Root" };
    ParamChoice loopMode { "Loop" };
    ParamToggle reverse { "Reverse" }, keytrack { "Keytrack" };
    juce::TextButton loadButton { "Load..." }, clearButton { "Clear" };
    juce::Label sampleInfo;
    std::unique_ptr<juce::FileChooser> chooser;

    std::vector<ParamKnob*> knobs();
};
} // namespace ferrite
