#include "LookAndFeel.h"

namespace loom
{
juce::Colour Colours::forOperator (int op)
{
    static const juce::Colour c[] = {
        juce::Colour (0xff5b5bd6), juce::Colour (0xffe5484d), juce::Colour (0xfff76b15),
        juce::Colour (0xff30a46c), juce::Colour (0xff0ea5b7), juce::Colour (0xffd6409f)
    };
    return c[juce::jlimit (0, 5, op)];
}

juce::Font LoomLookAndFeel::font (float size, bool bold)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

LoomLookAndFeel::LoomLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, Colours::bg);
    setColour (juce::Label::textColourId, Colours::text);
    setColour (juce::Slider::textBoxTextColourId, Colours::textDim);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::backgroundColourId, Colours::panelHi);
    setColour (juce::ComboBox::textColourId, Colours::text);
    setColour (juce::ComboBox::outlineColourId, Colours::outline);
    setColour (juce::ComboBox::arrowColourId, Colours::textDim);
    setColour (juce::PopupMenu::backgroundColourId, Colours::panel);
    setColour (juce::PopupMenu::textColourId, Colours::text);
    setColour (juce::PopupMenu::headerTextColourId, Colours::textDim);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::accent.withAlpha (0.12f));
    setColour (juce::PopupMenu::highlightedTextColourId, Colours::text);
    setColour (juce::TextButton::buttonColourId, Colours::panelHi);
    setColour (juce::TextButton::buttonOnColourId, Colours::panel);
    setColour (juce::TextButton::textColourOffId, Colours::text);
    setColour (juce::TextButton::textColourOnId, Colours::text);
    setColour (juce::TextEditor::backgroundColourId, Colours::panelHi);
    setColour (juce::TextEditor::textColourId, Colours::text);
    setColour (juce::TextEditor::highlightColourId, Colours::accent.withAlpha (0.25f));
    setColour (juce::TextEditor::outlineColourId, Colours::outline);
    setColour (juce::TextEditor::focusedOutlineColourId, Colours::accent);
    setColour (juce::CaretComponent::caretColourId, Colours::text);
    setColour (juce::AlertWindow::backgroundColourId, Colours::panel);
    setColour (juce::AlertWindow::textColourId, Colours::text);
    setColour (juce::AlertWindow::outlineColourId, Colours::outline);
}

static juce::Colour accentOf (juce::Component& c)
{
    const auto v = c.getProperties()["accent"];
    return v.isVoid() ? Colours::accent : juce::Colour::fromString (v.toString());
}

void LoomLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (2.0f);
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float lineW = juce::jmax (2.5f, r * 0.13f);
    const float arcR = r - lineW * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto accent = accentOf (s);

    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0, startAngle, endAngle, true);
    g.setColour (Colours::track);
    g.strokePath (track, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const bool bipolar = s.getMinimum() < 0 && s.getMaximum() > 0;
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    if (s.isEnabled() && std::abs (angle - from) > 0.001f)
    {
        juce::Path val;
        val.addCentredArc (c.x, c.y, arcR, arcR, 0, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (accent);
        g.strokePath (val, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // белая «шайба» с мягкой тенью
    const float knobR = r - lineW - 3.0f;
    juce::DropShadow (juce::Colours::black.withAlpha (0.10f), 4, { 0, 1 })
        .drawForPath (g, [&] { juce::Path p; p.addEllipse (c.x - knobR, c.y - knobR, knobR * 2, knobR * 2); return p; }());
    g.setColour (Colours::panel);
    g.fillEllipse (c.x - knobR, c.y - knobR, knobR * 2, knobR * 2);
    g.setColour (Colours::outline);
    g.drawEllipse (c.x - knobR, c.y - knobR, knobR * 2, knobR * 2, 1.0f);

    // указатель
    const float p0 = knobR * 0.25f, p1 = knobR * 0.78f;
    const auto dir = juce::Point<float> (std::sin (angle), -std::cos (angle));
    g.setColour (s.isEnabled() ? Colours::text : Colours::textDim);
    g.drawLine ({ c + dir * p0, c + dir * p1 }, juce::jmax (1.6f, r * 0.07f));
}

void LoomLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                        juce::Slider::SliderStyle style, juce::Slider& s)
{
    auto b = juce::Rectangle<int> (x, y, w, h).toFloat();
    if (style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearBar)
    {
        auto track = b.withSizeKeepingCentre (b.getWidth(), 4.0f);
        g.setColour (Colours::track);
        g.fillRoundedRectangle (track, 2.0f);
        const bool bipolar = s.getMinimum() < 0 && s.getMaximum() > 0;
        const float mid = bipolar ? (float) s.getPositionOfValue (0.0) : track.getX();
        g.setColour (accentOf (s));
        g.fillRoundedRectangle (juce::Rectangle<float> (juce::jmin (mid, pos), track.getY(), std::abs (pos - mid), 4.0f), 2.0f);
        g.setColour (Colours::panel);
        g.fillEllipse (pos - 6.0f, b.getCentreY() - 6.0f, 12.0f, 12.0f);
        g.setColour (Colours::outline.darker (0.15f));
        g.drawEllipse (pos - 6.0f, b.getCentreY() - 6.0f, 12.0f, 12.0f, 1.0f);
    }
    else
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, style, s);
    }
}

void LoomLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const float rad = r.getHeight() * 0.5f;
    const bool on = b.getToggleState();
    auto base = on ? Colours::panel : Colours::panelHi;
    if (over) base = base.darker (0.03f);
    if (down) base = base.darker (0.07f);
    if (on)
        juce::DropShadow (juce::Colours::black.withAlpha (0.10f), 4, { 0, 1 })
            .drawForRectangle (g, r.toNearestInt());
    g.setColour (base);
    g.fillRoundedRectangle (r, rad);
    if (! b.getProperties()["noOutline"])
    {
        g.setColour (Colours::outline);
        g.drawRoundedRectangle (r, rad, 1.0f);
    }
}

void LoomLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.isEnabled() ? Colours::text : Colours::textDim);
    g.drawText (b.getButtonText(), b.getLocalBounds().reduced (6, 0), juce::Justification::centred, true);
}

void LoomLookAndFeel::drawSwitch (juce::Graphics& g, juce::Rectangle<float> r, bool on, juce::Colour onColour)
{
    const float rad = r.getHeight() * 0.5f;
    g.setColour (on ? onColour : Colours::track.darker (0.05f));
    g.fillRoundedRectangle (r, rad);
    const float d = r.getHeight() - 4.0f;
    const float kx = on ? r.getRight() - d - 2.0f : r.getX() + 2.0f;
    g.setColour (juce::Colours::white);
    g.fillEllipse (kx, r.getY() + 2.0f, d, d);
}

void LoomLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const float sh = juce::jmin (16.0f, r.getHeight() - 2.0f);
    auto sw = juce::Rectangle<float> (r.getX() + 1, r.getCentreY() - sh * 0.5f, sh * 1.75f, sh);
    drawSwitch (g, sw, b.getToggleState(), b.isEnabled() ? accentOf (b) : Colours::textDim);
    g.setColour (b.isEnabled() ? Colours::text : Colours::textDim);
    g.setFont (font (12.0f));
    g.drawText (b.getButtonText(), r.withLeft (sw.getRight() + 6), juce::Justification::centredLeft);
}

void LoomLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<int> (0, 0, w, h).toFloat().reduced (0.5f);
    const float rad = juce::jmin (r.getHeight() * 0.5f, 10.0f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, rad);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (r, rad, 1.0f);
    const float ax = (float) w - 13.0f, ay = (float) h * 0.5f;
    juce::Path arrow;
    arrow.startNewSubPath (ax - 3.5f, ay - 1.5f);
    arrow.lineTo (ax, ay + 2.0f);
    arrow.lineTo (ax + 3.5f, ay - 1.5f);
    g.setColour (Colours::textDim);
    g.strokePath (arrow, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void LoomLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 26, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void LoomLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (Colours::panel);
    g.setColour (Colours::outline);
    g.drawRect (0, 0, w, h);
}

juce::Font LoomLookAndFeel::getComboBoxFont (juce::ComboBox&) { return font (12.5f); }
juce::Font LoomLookAndFeel::getLabelFont (juce::Label& l)   { return l.getFont(); }
juce::Font LoomLookAndFeel::getPopupMenuFont()               { return font (13.5f); }
juce::Font LoomLookAndFeel::getTextButtonFont (juce::TextButton&, int h) { return font (juce::jmin (12.5f, (float) h * 0.55f)); }

juce::Label* LoomLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (font (11.0f));
    l->setColour (juce::Label::textColourId, Colours::textDim);
    l->setColour (juce::Label::outlineWhenEditingColourId, Colours::accent);
    l->setColour (juce::Label::backgroundWhenEditingColourId, Colours::panel);
    l->setColour (juce::TextEditor::textColourId, Colours::text);
    return l;
}

void LoomLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& l)
{
    if (l.isBeingEdited()) { LookAndFeel_V4::drawLabel (g, l); return; }
    g.setColour (l.findColour (juce::Label::textColourId));
    g.setFont (l.getFont());
    g.drawFittedText (l.getText(), l.getLocalBounds(), l.getJustificationType(), 1, 0.9f);
}
} // namespace loom
