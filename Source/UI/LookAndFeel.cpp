#include "LookAndFeel.h"

namespace ferrite
{
juce::Colour Colours::forOperator (int op)
{
    static const juce::Colour c[] = {
        juce::Colour (0xffff8a3d), juce::Colour (0xfff2c14e), juce::Colour (0xff8bd450),
        juce::Colour (0xff4fc3c9), juce::Colour (0xff7b8cff), juce::Colour (0xffd36bd8)
    };
    return c[juce::jlimit (0, 5, op)];
}

juce::Font FerriteLookAndFeel::font (float size, bool bold)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

FerriteLookAndFeel::FerriteLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, Colours::bg);
    setColour (juce::Label::textColourId, Colours::text);
    setColour (juce::Slider::textBoxTextColourId, Colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::backgroundColourId, Colours::panelHi);
    setColour (juce::ComboBox::textColourId, Colours::text);
    setColour (juce::ComboBox::outlineColourId, Colours::outline);
    setColour (juce::ComboBox::arrowColourId, Colours::textDim);
    setColour (juce::PopupMenu::backgroundColourId, Colours::panelHi);
    setColour (juce::PopupMenu::textColourId, Colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::accent.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, Colours::text);
    setColour (juce::TextButton::buttonColourId, Colours::panelHi);
    setColour (juce::TextButton::buttonOnColourId, Colours::accent);
    setColour (juce::TextButton::textColourOffId, Colours::text);
    setColour (juce::TextButton::textColourOnId, Colours::bg);
    setColour (juce::TextEditor::backgroundColourId, Colours::panelHi);
    setColour (juce::TextEditor::textColourId, Colours::text);
    setColour (juce::TextEditor::outlineColourId, Colours::outline);
    setColour (juce::TextEditor::focusedOutlineColourId, Colours::accent);
    setColour (juce::AlertWindow::backgroundColourId, Colours::panel);
    setColour (juce::AlertWindow::textColourId, Colours::text);
    setColour (juce::AlertWindow::outlineColourId, Colours::outline);
}

void FerriteLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float startAngle, float endAngle, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float lineW = juce::jmax (2.0f, r * 0.14f);
    const float arcR = r - lineW * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto accentVar = s.getProperties()["accent"];
    const auto accent = accentVar.isVoid() ? Colours::accent : juce::Colour::fromString (accentVar.toString());

    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0, startAngle, endAngle, true);
    g.setColour (Colours::outline);
    g.strokePath (track, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // биполярные ручки рисуем от центра
    const bool bipolar = s.getMinimum() < 0 && s.getMaximum() > 0;
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    if (s.isEnabled())
    {
        juce::Path val;
        val.addCentredArc (c.x, c.y, arcR, arcR, 0, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (accent);
        g.strokePath (val, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float knobR = r * 0.62f;
    g.setColour (Colours::panelHi);
    g.fillEllipse (c.x - knobR, c.y - knobR, knobR * 2, knobR * 2);
    g.setColour (Colours::outline.brighter (0.2f));
    g.drawEllipse (c.x - knobR, c.y - knobR, knobR * 2, knobR * 2, 1.0f);

    juce::Path ptr;
    ptr.addRoundedRectangle (-lineW * 0.35f, -knobR, lineW * 0.7f, knobR * 0.55f, lineW * 0.3f);
    g.setColour (s.isEnabled() ? Colours::text : Colours::textDim);
    g.fillPath (ptr, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

void FerriteLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                           juce::Slider::SliderStyle style, juce::Slider& s)
{
    auto b = juce::Rectangle<int> (x, y, w, h).toFloat();
    if (style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearBar)
    {
        auto track = b.withSizeKeepingCentre (b.getWidth(), 4.0f);
        g.setColour (Colours::outline);
        g.fillRoundedRectangle (track, 2.0f);
        const bool bipolar = s.getMinimum() < 0 && s.getMaximum() > 0;
        const float mid = bipolar ? (float) s.getPositionOfValue (0.0) : track.getX();
        const auto accentVar = s.getProperties()["accent"];
        g.setColour (accentVar.isVoid() ? Colours::accent : juce::Colour::fromString (accentVar.toString()));
        g.fillRoundedRectangle (juce::Rectangle<float> (juce::jmin (mid, pos), track.getY(), std::abs (pos - mid), 4.0f), 2.0f);
        g.setColour (Colours::text);
        g.fillEllipse (pos - 5.0f, b.getCentreY() - 5.0f, 10.0f, 10.0f);
    }
    else
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, style, s);
    }
}

void FerriteLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto base = b.getToggleState() ? Colours::accent : Colours::panelHi;
    if (over) base = base.brighter (0.08f);
    if (down) base = base.darker (0.15f);
    g.setColour (base);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (Colours::outline);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void FerriteLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const float box = juce::jmin (14.0f, r.getHeight() - 2.0f);
    auto boxR = juce::Rectangle<float> (r.getX() + 2, r.getCentreY() - box * 0.5f, box, box);
    g.setColour (b.getToggleState() ? Colours::accent : Colours::panelHi.brighter (over ? 0.1f : 0.0f));
    g.fillRoundedRectangle (boxR, 3.0f);
    g.setColour (Colours::outline.brighter (0.3f));
    g.drawRoundedRectangle (boxR, 3.0f, 1.0f);
    g.setColour (Colours::text);
    g.setFont (font (12.0f));
    g.drawText (b.getButtonText(), r.withLeft (boxR.getRight() + 5), juce::Justification::centredLeft);
}

void FerriteLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<int> (0, 0, w, h).toFloat().reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (Colours::textDim);
    g.fillPath (arrow);
}

juce::Font FerriteLookAndFeel::getComboBoxFont (juce::ComboBox&) { return font (13.0f); }
juce::Font FerriteLookAndFeel::getLabelFont (juce::Label& l)   { return l.getFont(); }
juce::Font FerriteLookAndFeel::getPopupMenuFont()               { return font (14.0f); }
juce::Font FerriteLookAndFeel::getTextButtonFont (juce::TextButton&, int h) { return font (juce::jmin (13.0f, (float) h * 0.6f)); }

juce::Label* FerriteLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (font (11.0f));
    l->setColour (juce::Label::textColourId, Colours::textDim);
    l->setColour (juce::Label::outlineWhenEditingColourId, Colours::accent);
    l->setColour (juce::Label::backgroundWhenEditingColourId, Colours::panelHi);
    return l;
}

void FerriteLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& l)
{
    if (l.isBeingEdited()) { LookAndFeel_V4::drawLabel (g, l); return; }
    g.setColour (l.findColour (juce::Label::textColourId));
    g.setFont (l.getFont());
    g.drawFittedText (l.getText(), l.getLocalBounds(), l.getJustificationType(), 1, 0.9f);
}
} // namespace ferrite
