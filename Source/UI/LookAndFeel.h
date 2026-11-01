#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

namespace mist
{
// Все цвета интерфейса в одном месте
inline juce::String midDot() { return juce::String::fromUTF8 ("  \xc2\xb7  "); }

// Цвета текущей темы. Меняются целиком через applyTheme() — все компоненты читают их при отрисовке.
namespace Colours
{
    inline juce::Colour bg, panel, panelHi, outline, track, text, textDim, accent, accent2, good, danger, vizBg;
    inline juce::Colour opColours[6];
    juce::Colour forOperator (int op);
}

enum class ThemeId { Light = 0, Dark = 1 };
struct Theme
{
    juce::Colour bg, panel, panelHi, outline, track, text, textDim, accent, good, danger, vizBg;
    std::array<juce::Colour, 6> ops;
    float shadowAlpha;
};
const Theme& getTheme (ThemeId id);
void applyTheme (ThemeId id);       // переписывает Colours::*
ThemeId currentTheme();

class MistLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MistLookAndFeel();

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

    // Шрифты: Medeo Sans — интерфейс, Medeo Mono — цифры/значения, Medeo Pixel — логотип
    static juce::Font font (float size, bool bold = false);
    static juce::Font mono (float size, bool medium = false);
    static juce::Font pixel (float size);
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;
    void refreshColours();   // после applyTheme()
    static void drawSwitch (juce::Graphics&, juce::Rectangle<float> r, bool on, juce::Colour onColour);
};
} // namespace mist
