#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "LookAndFeel.h"

namespace ferrite
{
using APVTS = juce::AudioProcessorValueTreeState;

// Ручка с подписью и значением, привязанная к параметру
class ParamKnob : public juce::Component
{
public:
    ParamKnob (const juce::String& label, juce::Colour accent = Colours::accent);
    void attach (APVTS& state, const juce::String& paramId);
    void detach();
    void resized() override;
    juce::Slider slider;
    juce::Label label;

private:
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

class ParamToggle : public juce::ToggleButton
{
public:
    explicit ParamToggle (const juce::String& text) : juce::ToggleButton (text) {}
    void attach (APVTS& state, const juce::String& paramId)
    {
        attachment.reset();
        attachment = std::make_unique<APVTS::ButtonAttachment> (state, paramId, *this);
    }
    void detach() { attachment.reset(); }

private:
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

class ParamChoice : public juce::Component
{
public:
    explicit ParamChoice (const juce::String& label = {});
    void attach (APVTS& state, const juce::String& paramId);
    void detach() { attachment.reset(); }
    void resized() override;
    juce::ComboBox box;
    juce::Label label;

private:
    std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
};

// Панель с заголовком
class Section : public juce::Component
{
public:
    explicit Section (const juce::String& title, juce::Colour accent = Colours::accent) : title (title), accent (accent) {}
    void paint (juce::Graphics& g) override;
    juce::Rectangle<int> getContentBounds() const { return getLocalBounds().reduced (8).withTrimmedTop (18); }
    void setTitle (const juce::String& t) { title = t; repaint(); }
    void setAccent (juce::Colour c) { accent = c; repaint(); }

private:
    juce::String title;
    juce::Colour accent;
};

// Раскладка ручек в ряд/сетку
void layoutRow (juce::Rectangle<int> area, std::initializer_list<juce::Component*> items, int gap = 4);
void layoutGrid (juce::Rectangle<int> area, int cols, const std::vector<juce::Component*>& items, int gap = 4);

// Рисует DAHDSR по значениям
class EnvelopeView : public juce::Component
{
public:
    void setValues (float d, float a, float h, float dec, float s, float r) { vals = { d, a, h, dec, s, r }; repaint(); }
    void setColour (juce::Colour c) { colour = c; repaint(); }
    void paint (juce::Graphics& g) override;

private:
    std::array<float, 6> vals {};
    juce::Colour colour = Colours::accent;
};

class ScopeView : public juce::Component
{
public:
    void push (const float* data, int n);
    void paint (juce::Graphics& g) override;

private:
    std::vector<float> buf = std::vector<float> (1024, 0.0f);
    int write = 0;
};
} // namespace ferrite
