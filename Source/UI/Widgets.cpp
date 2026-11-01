#include "Widgets.h"

namespace mist
{
ParamKnob::ParamKnob (const juce::String& text, juce::Colour accent)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    slider.getProperties().set ("accent", accent == Colours::accent ? juce::String ("accent") : accent.toString());
    slider.setDoubleClickReturnValue (true, 0.0);
    slider.setMouseDragSensitivity (220);
    slider.setWantsKeyboardFocus (true);
    slider.setTitle (text);
    slider.onValueChange = [this] { updateValue(); };
    addAndMakeVisible (slider);

    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (MistLookAndFeel::font (MistLookAndFeel::Type::subheadline, true));
    styleLabel (label, false);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);

    value.setJustificationType (juce::Justification::centred);
    value.setFont (MistLookAndFeel::mono (MistLookAndFeel::Type::footnote));
    styleLabel (value, true);
    value.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (value);
}

void ParamKnob::updateValue()
{
    value.setText (slider.getTextFromValue (slider.getValue()), juce::dontSendNotification);
}

void ParamKnob::attach (APVTS& state, const juce::String& id)
{
    attachment.reset();
    attachment = std::make_unique<APVTS::SliderAttachment> (state, id, slider);
    if (auto* p = state.getParameter (id))
    {
        slider.setTitle (p->getName (64));
        slider.setDescription (label.getText());
    }
    if (auto* p = state.getParameter (id))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    updateValue();
}

void ParamKnob::detach() { attachment.reset(); }

void ParamKnob::resized()
{
    // [круг] [имя] [значение] — по центру ячейки
    auto r = getLocalBounds();
    const int nameH = 15, valH = 13;
    const int knob = juce::jlimit (20, 54, juce::jmin (r.getWidth() - 6, r.getHeight() - nameH - valH - 4));
    auto area = r.withSizeKeepingCentre (r.getWidth(), knob + 4 + nameH + valH);
    slider.setBounds (area.removeFromTop (knob).withSizeKeepingCentre (knob, knob));
    area.removeFromTop (4);
    label.setBounds (area.removeFromTop (nameH));
    value.setBounds (area.removeFromTop (valH));
}

ParamChoice::ParamChoice (const juce::String& text)
{
    addAndMakeVisible (box);
    if (text.isNotEmpty())
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (MistLookAndFeel::font (MistLookAndFeel::Type::subheadline, true));
        styleLabel (label, true);
        label.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (label);
    }
}

void ParamChoice::attach (APVTS& state, const juce::String& id)
{
    attachment.reset();
    box.clear (juce::dontSendNotification);
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (id)))
        box.addItemList (p->choices, 1);
    if (auto* p = state.getParameter (id)) box.setTitle (p->getName (64));
    attachment = std::make_unique<APVTS::ComboBoxAttachment> (state, id, box);
}

void ParamChoice::resized()
{
    auto r = getLocalBounds();
    if (label.getText().isNotEmpty())
        label.setBounds (r.removeFromTop (15));
    box.setBounds (r.withSizeKeepingCentre (r.getWidth(), juce::jmin (r.getHeight(), 26)));
}

void drawCard (juce::Graphics& g, juce::Rectangle<float> r, float radius, bool shadow)
{
    juce::ignoreUnused (shadow);   // теней нет: плотная поверхность + рамка 1 px
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (r, radius);
    g.setColour (Colours::outline);
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
}

void Section::paint (juce::Graphics& g)
{
    if (flat) return;
    drawCard (g, getLocalBounds().toFloat(), 12.0f, false);
    g.setColour (Colours::text);
    g.setFont (MistLookAndFeel::font (MistLookAndFeel::Type::headline, true));
    g.drawText (title, juce::Rectangle<int> (12, 7, getWidth() - 24, 18), juce::Justification::centredLeft);
}

void styleLabel (juce::Label& l, bool dim)
{
    l.getProperties().set ("dim", dim);
    l.setColour (juce::Label::textColourId, dim ? Colours::textDim : Colours::text);
}

void disableClickFocus (juce::Component& root)
{
    root.setMouseClickGrabsKeyboardFocus (false);
    for (auto* c : root.getChildren()) disableClickFocus (*c);
}

void restyleTree (juce::Component& root)
{
    if (auto* l = dynamic_cast<juce::Label*> (&root))
        if (l->getProperties().contains ("dim"))
            l->setColour (juce::Label::textColourId, (bool) l->getProperties()["dim"] ? Colours::textDim : Colours::text);
    for (auto* c : root.getChildren()) restyleTree (*c);
    root.repaint();
}

void layoutRow (juce::Rectangle<int> area, std::initializer_list<juce::Component*> items, int gap)
{
    layoutRow (area, std::vector<juce::Component*> (items), gap);
}

void layoutRow (juce::Rectangle<int> area, const std::vector<juce::Component*>& items, int gap, int maxWidth)
{
    const int n = (int) items.size();
    if (n == 0) return;
    const int w = juce::jmin (maxWidth, (area.getWidth() - gap * (n - 1)) / n);
    for (auto* c : items)
    {
        if (c != nullptr) c->setBounds (area.removeFromLeft (w));
        area.removeFromLeft (gap);
    }
}

void layoutGrid (juce::Rectangle<int> area, int cols, const std::vector<juce::Component*>& items, int gap)
{
    const int rows = ((int) items.size() + cols - 1) / cols;
    if (rows == 0) return;
    const int w = (area.getWidth() - gap * (cols - 1)) / cols;
    const int h = (area.getHeight() - gap * (rows - 1)) / rows;
    for (size_t i = 0; i < items.size(); ++i)
    {
        const int c = (int) i % cols, r = (int) i / cols;
        if (items[i] != nullptr)
            items[i]->setBounds (area.getX() + c * (w + gap), area.getY() + r * (h + gap), w, h);
    }
}

void EnvelopeView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (Colours::panelHi);
    g.fillRoundedRectangle (r, 8.0f);

    auto vis = [] (float t) { return std::sqrt (juce::jmax (0.0f, t)) + 0.02f; };
    const float d = vis (vals[0]), a = vis (vals[1]), h = vis (vals[2]), dec = vis (vals[3]), rel = vis (vals[5]);
    const float sus = vals[4];
    const float susLen = 0.6f;
    const float total = d + a + h + dec + susLen + rel;
    auto X = [&] (float t) { return r.getX() + 10 + (r.getWidth() - 20) * t / total; };
    auto Y = [&] (float v) { return r.getBottom() - 10 - (r.getHeight() - 20) * v; };

    g.setColour (Colours::outline);
    for (int i = 1; i < 4; ++i) g.drawHorizontalLine ((int) Y ((float) i / 4.0f), r.getX() + 6, r.getRight() - 6);

    juce::Path p;
    float t = 0;
    p.startNewSubPath (X (0), Y (0));
    t += d;   p.lineTo (X (t), Y (0));
    t += a;   p.lineTo (X (t), Y (1));
    t += h;   p.lineTo (X (t), Y (1));
    const float t0 = t;
    for (int i = 1; i <= 24; ++i)
    {
        const float k = (float) i / 24.0f;
        p.lineTo (X (t0 + dec * k), Y (sus + (1 - sus) * std::exp (-5.0f * k)));
    }
    t += dec;
    t += susLen; p.lineTo (X (t), Y (sus));
    const float t1 = t;
    for (int i = 1; i <= 24; ++i)
    {
        const float k = (float) i / 24.0f;
        p.lineTo (X (t1 + rel * k), Y (sus * std::exp (-5.0f * k)));
    }

    juce::Path fill (p);
    fill.lineTo (X (total), Y (0));
    fill.closeSubPath();
    g.setColour (colour.withAlpha (0.10f));
    g.fillPath (fill);
    g.setColour (colour);
    g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
} // namespace mist
