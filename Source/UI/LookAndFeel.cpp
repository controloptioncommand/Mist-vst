#include "LookAndFeel.h"
#include "BinaryData.h"

namespace mist
{
juce::Colour Colours::forOperator (int op)
{
    return opColours[juce::jlimit (0, 5, op)];
}

// ---------------------------------------------------------------------------
// Темы. ВРЕМЕННЫЕ палитры — будут заменены палитрами пользователя.
const Theme& getTheme (ThemeId id)
{
    static const Theme light {
        juce::Colour (0xfff2f2f4), juce::Colour (0xffffffff), juce::Colour (0xfff5f5f7), juce::Colour (0xffe3e3e8),
        juce::Colour (0xffe6e6eb), juce::Colour (0xff1c1c1e), juce::Colour (0xff8e8e93), juce::Colour (0xff1e88ff),
        juce::Colour (0xff34c759), juce::Colour (0xffe5484d), juce::Colour (0xff0d0d12),
        { juce::Colour (0xff5b5bd6), juce::Colour (0xffe5484d), juce::Colour (0xfff76b15),
          juce::Colour (0xff30a46c), juce::Colour (0xff0ea5b7), juce::Colour (0xffd6409f) },
        0.06f };
    static const Theme dark {
        juce::Colour (0xff0e0e10), juce::Colour (0xff17171a), juce::Colour (0xff1f1f23), juce::Colour (0xff2a2a2f),
        juce::Colour (0xff2c2c31), juce::Colour (0xffececf0), juce::Colour (0xff8a8a92), juce::Colour (0xff3d9bff),
        juce::Colour (0xff30d158), juce::Colour (0xffff5a5f), juce::Colour (0xff08080a),
        { juce::Colour (0xff7b7bf0), juce::Colour (0xffff6369), juce::Colour (0xffff8b3d),
          juce::Colour (0xff3dd68c), juce::Colour (0xff22c3d6), juce::Colour (0xffeb6bc0) },
        0.35f };
    return id == ThemeId::Dark ? dark : light;
}

static ThemeId gTheme = ThemeId::Light;
ThemeId currentTheme() { return gTheme; }

void applyTheme (ThemeId id)
{
    gTheme = id;
    const auto& t = getTheme (id);
    Colours::bg = t.bg; Colours::panel = t.panel; Colours::panelHi = t.panelHi; Colours::outline = t.outline;
    Colours::track = t.track; Colours::text = t.text; Colours::textDim = t.textDim; Colours::accent = t.accent;
    Colours::accent2 = t.accent; Colours::good = t.good; Colours::danger = t.danger; Colours::vizBg = t.vizBg;
    for (int i = 0; i < 6; ++i) Colours::opColours[i] = t.ops[(size_t) i];
}

// ---------------------------------------------------------------------------
// Шрифты из BinaryData
namespace
{
struct Typefaces
{
    juce::Typeface::Ptr sans, sansSemi, mono, monoMedium, pixel;
    Typefaces()
    {
        auto load = [] (const void* d, int n) { return juce::Typeface::createSystemTypefaceFor (d, (size_t) n); };
        sans       = load (MistFonts::MedeoSansRegular_ttf,  MistFonts::MedeoSansRegular_ttfSize);
        sansSemi   = load (MistFonts::MedeoSansSemiBold_ttf, MistFonts::MedeoSansSemiBold_ttfSize);
        mono       = load (MistFonts::MedeoMonoRegular_ttf,  MistFonts::MedeoMonoRegular_ttfSize);
        monoMedium = load (MistFonts::MedeoMonoMedium_ttf,   MistFonts::MedeoMonoMedium_ttfSize);
        pixel      = load (MistFonts::MedeoPixelRegular_ttf, MistFonts::MedeoPixelRegular_ttfSize);
    }
};
const Typefaces& typefaces() { static const Typefaces t; return t; }

juce::Font makeFont (juce::Typeface::Ptr tf, float size)
{
    return juce::Font (juce::FontOptions (tf).withHeight (size));
}
} // namespace

juce::Font MistLookAndFeel::font (float size, bool bold)
{
    return makeFont (bold ? typefaces().sansSemi : typefaces().sans, size);
}

juce::Font MistLookAndFeel::mono (float size, bool medium)
{
    return makeFont (medium ? typefaces().monoMedium : typefaces().mono, size);
}

juce::Font MistLookAndFeel::pixel (float size)
{
    return makeFont (typefaces().pixel, size);
}

juce::Typeface::Ptr MistLookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    // всё, что рисуется шрифтом «по умолчанию» (меню, диалоги, редакторы), — Medeo Sans
    if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName() || f.getTypefaceName().isEmpty())
        return f.isBold() ? typefaces().sansSemi : typefaces().sans;
    return LookAndFeel_V4::getTypefaceForFont (f);
}

MistLookAndFeel::MistLookAndFeel()
{
    if (Colours::bg == juce::Colour()) applyTheme (ThemeId::Light);
    refreshColours();
}

void MistLookAndFeel::refreshColours()
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
    const auto v = c.getProperties()["accent"].toString();
    if (v.isEmpty() || v == "accent") return Colours::accent;
    if (v == "good") return Colours::good;
    if (v.startsWith ("op")) return Colours::forOperator (v.getTrailingIntValue());
    return juce::Colour::fromString (v);
}

void MistLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
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

void MistLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
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

void MistLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
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

void MistLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.isEnabled() ? Colours::text : Colours::textDim);
    g.drawText (b.getButtonText(), b.getLocalBounds().reduced (6, 0), juce::Justification::centred, true);
}

void MistLookAndFeel::drawSwitch (juce::Graphics& g, juce::Rectangle<float> r, bool on, juce::Colour onColour)
{
    const float rad = r.getHeight() * 0.5f;
    g.setColour (on ? onColour : Colours::track.darker (0.05f));
    g.fillRoundedRectangle (r, rad);
    const float d = r.getHeight() - 4.0f;
    const float kx = on ? r.getRight() - d - 2.0f : r.getX() + 2.0f;
    g.setColour (juce::Colours::white);
    g.fillEllipse (kx, r.getY() + 2.0f, d, d);
}

void MistLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const float sh = juce::jmin (16.0f, r.getHeight() - 2.0f);
    auto sw = juce::Rectangle<float> (r.getX() + 1, r.getCentreY() - sh * 0.5f, sh * 1.75f, sh);
    drawSwitch (g, sw, b.getToggleState(), b.isEnabled() ? accentOf (b) : Colours::textDim);
    g.setColour (b.isEnabled() ? Colours::text : Colours::textDim);
    g.setFont (font (12.0f));
    g.drawText (b.getButtonText(), r.withLeft (sw.getRight() + 6), juce::Justification::centredLeft);
}

void MistLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
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

void MistLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 26, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void MistLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (Colours::panel);
    g.setColour (Colours::outline);
    g.drawRect (0, 0, w, h);
}

juce::Font MistLookAndFeel::getComboBoxFont (juce::ComboBox&) { return font (12.5f); }
juce::Font MistLookAndFeel::getLabelFont (juce::Label& l)   { return l.getFont(); }
juce::Font MistLookAndFeel::getPopupMenuFont()               { return font (13.5f); }
juce::Font MistLookAndFeel::getTextButtonFont (juce::TextButton&, int h) { return font (juce::jmin (12.5f, (float) h * 0.55f)); }

juce::Label* MistLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (font (11.0f));
    l->setColour (juce::Label::textColourId, Colours::textDim);
    l->setColour (juce::Label::outlineWhenEditingColourId, Colours::accent);
    l->setColour (juce::Label::backgroundWhenEditingColourId, Colours::panel);
    l->setColour (juce::TextEditor::textColourId, Colours::text);
    return l;
}

void MistLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& l)
{
    if (l.isBeingEdited()) { LookAndFeel_V4::drawLabel (g, l); return; }
    g.setColour (l.findColour (juce::Label::textColourId));
    g.setFont (l.getFont());
    g.drawFittedText (l.getText(), l.getLocalBounds(), l.getJustificationType(), 1, 0.9f);
}
} // namespace mist
