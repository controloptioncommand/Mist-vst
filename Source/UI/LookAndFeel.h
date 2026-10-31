#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace loom
{
// Все цвета интерфейса в одном месте
inline juce::String midDot() { return juce::String::fromUTF8 ("  \xc2\xb7  "); }

namespace Colours
{
    const juce::Colour bg        { 0xfff2f2f4 };   // фон окна
    const juce::Colour panel     { 0xffffffff };   // карточки
    const juce::Colour panelHi   { 0xfff5f5f7 };   // вторичные поверхности, кнопки
    const juce::Colour outline   { 0xffe3e3e8 };
    const juce::Colour track     { 0xffe6e6eb };   // дорожка ручек
    const juce::Colour text      { 0xff1c1c1e };
    const juce::Colour textDim   { 0xff8e8e93 };
    const juce::Colour accent    { 0xff1e88ff };   // синий — основной
    const juce::Colour accent2   { 0xff1e88ff };   // модуляция/фильтр — тот же синий
    const juce::Colour good      { 0xff34c759 };   // зелёный (meter, drone)
    const juce::Colour danger    { 0xffe5484d };
    const juce::Colour vizBg     { 0xff0d0d12 };   // тёмные режимы визуализации

    juce::Colour forOperator (int op);
}

class LoomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    LoomLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;

    static juce::Font font (float size, bool bold = false);
    static void drawSwitch (juce::Graphics&, juce::Rectangle<float> r, bool on, juce::Colour onColour);
};
} // namespace loom
