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
void restyleTree (juce::Component& root);
// клик мышью не забирает фокус клавиатуры — кольцо фокуса видно только при навигации Tab
void disableClickFocus (juce::Component& root);   // обновить цвета всех подписей после смены темы

void layoutRow (juce::Rectangle<int> area, std::initializer_list<juce::Component*> items, int gap = 4);
void layoutRow (juce::Rectangle<int> area, const std::vector<juce::Component*>& items, int gap = 4, int maxWidth = 10000);
void layoutGrid (juce::Rectangle<int> area, int cols, const std::vector<juce::Component*>& items, int gap = 4);

// Белая карточка с тенью — общий фон блоков
void drawCard (juce::Graphics& g, juce::Rectangle<float> r, float radius = 12.0f, bool shadow = true);

// График огибающей. На середине атаки, спада и релиза — точки изгиба: тянуть вверх/вниз,
// двойной клик — форма по умолчанию. С клавиатуры: ←/→ выбор отрезка, ↑/↓ изгиб.
class EnvelopeView : public juce::Component
{
public:
    EnvelopeView();
    struct Ids { juce::String delay, attack, hold, decay, sustain, release, aCurve, dCurve, rCurve; };
    void attach (APVTS& state, const Ids& ids);
    void setColour (juce::Colour c) { colour = c; hasColour = true; repaint(); }   // иначе — Accent текущей темы
    void refresh();                    // перечитать параметры (по таймеру редактора)
    void paint (juce::Graphics& g) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent&) override { setHover (-1); }
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    bool keyPressed (const juce::KeyPress& k) override;
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { repaint(); }

private:
    float value (const juce::String& id) const;
    juce::RangedAudioParameter* curveParam (int seg) const;
    void nudgeCurve (int seg, float delta);
    int handleAt (juce::Point<float> p) const;
    void setHover (int h) { if (h != hover) { hover = h; repaint(); } }
    void updateDescription();

    APVTS* state = nullptr;
    Ids ids;
    std::array<float, 9> vals {};      // d a h dec s r curveA curveD curveR
    std::array<juce::Point<float>, 3> handles;
    int hover = -1, dragging = -1, focused = 0;
    float dragStartCurve = 0.0f;
    juce::Colour colour;
    bool hasColour = false;
};
} // namespace mist
