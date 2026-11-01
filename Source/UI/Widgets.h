#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "LookAndFeel.h"

namespace mist
{
using APVTS = juce::AudioProcessorValueTreeState;

// Ручка: сверху круг, под ним жирное имя и значение — как в макете
class ParamKnob : public juce::Component
{
public:
    explicit ParamKnob (const juce::String& label, juce::Colour accent = Colours::accent);
    void attach (APVTS& state, const juce::String& paramId);
    void detach();
    void setLabelText (const juce::String& n) { label.setText (n, juce::dontSendNotification); }
    void resized() override;
    juce::Slider slider;
    juce::Label label, value;

private:
    void updateValue();
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

// Карточка с заголовком. flat = без фона и заголовка (когда вложена в страницу)
class Section : public juce::Component
{
public:
    explicit Section (const juce::String& title, juce::Colour accent = Colours::accent) : title (title), accent (accent) {}
    void paint (juce::Graphics& g) override;
    juce::Rectangle<int> getContentBounds() const
    {
        return flat ? getLocalBounds() : getLocalBounds().reduced (12).withTrimmedTop (20);
    }
    void setTitle (const juce::String& t) { title = t; repaint(); }
    void setAccent (juce::Colour c) { accent = c; repaint(); }
    void setFlat (bool f) { flat = f; repaint(); resized(); }

private:
    juce::String title;
    juce::Colour accent;
    bool flat = false;
};

// Цвет подписи задаётся ролью, чтобы перекрашиваться при смене темы
void styleLabel (juce::Label& l, bool dim);
void restyleTree (juce::Component& root);   // обновить цвета всех подписей после смены темы

void layoutRow (juce::Rectangle<int> area, std::initializer_list<juce::Component*> items, int gap = 4);
void layoutRow (juce::Rectangle<int> area, const std::vector<juce::Component*>& items, int gap = 4, int maxWidth = 10000);
void layoutGrid (juce::Rectangle<int> area, int cols, const std::vector<juce::Component*>& items, int gap = 4);

// Белая карточка с тенью — общий фон блоков
void drawCard (juce::Graphics& g, juce::Rectangle<float> r, float radius = 12.0f, bool shadow = true);

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
} // namespace mist
