#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace ferrite
{
// Все цвета интерфейса в одном месте — при переносе дизайна правим здесь
namespace Colours
{
    const juce::Colour bg        { 0xff121316 };
    const juce::Colour panel     { 0xff1b1d22 };
    const juce::Colour panelHi   { 0xff23262d };
    const juce::Colour outline   { 0xff2e323a };
    const juce::Colour text      { 0xffd9dce2 };
    const juce::Colour textDim   { 0xff7d838f };
    const juce::Colour accent    { 0xffff8a3d };   // оранжевый
    const juce::Colour accent2   { 0xff4fc3c9 };   // бирюзовый
    const juce::Colour danger    { 0xffe0525a };

    juce::Colour forOperator (int op);
}

class FerriteLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FerriteLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;

    static juce::Font font (float size, bool bold = false);
};
} // namespace ferrite
