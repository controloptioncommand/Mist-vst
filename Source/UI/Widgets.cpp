#include "Widgets.h"

namespace ferrite
{
ParamKnob::ParamKnob (const juce::String& text, juce::Colour accent)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 14);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    slider.getProperties().set ("accent", accent.toString());
    slider.setPopupDisplayEnabled (false, false, nullptr);
    slider.setDoubleClickReturnValue (true, 0.0);
    addAndMakeVisible (slider);

    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (FerriteLookAndFeel::font (11.0f));
    label.setColour (juce::Label::textColourId, Colours::textDim);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);
}

void ParamKnob::attach (APVTS& state, const juce::String& id)
{
    attachment.reset();
    attachment = std::make_unique<APVTS::SliderAttachment> (state, id, slider);
    if (auto* p = state.getParameter (id))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
}

void ParamKnob::detach() { attachment.reset(); }

void ParamKnob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (13));
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, r.getWidth(), 14);
    // ограничиваем размер ручки, чтобы в больших ячейках они не раздувались
    const int maxKnob = 46;
    if (r.getHeight() - 14 > maxKnob)
        r = r.withSizeKeepingCentre (r.getWidth(), maxKnob + 14);
    slider.setBounds (r);
}

ParamChoice::ParamChoice (const juce::String& text)
{
    addAndMakeVisible (box);
    if (text.isNotEmpty())
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (FerriteLookAndFeel::font (11.0f));
        label.setColour (juce::Label::textColourId, Colours::textDim);
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
    attachment = std::make_unique<APVTS::ComboBoxAttachment> (state, id, box);
}

void ParamChoice::resized()
{
    auto r = getLocalBounds();
    if (label.isVisible() && label.getText().isNotEmpty())
        label.setBounds (r.removeFromTop (13));
    box.setBounds (r.withSizeKeepingCentre (r.getWidth(), juce::jmin (r.getHeight(), 24)));
}

void Section::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (Colours::outline);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    g.setColour (accent);
    g.fillRoundedRectangle (juce::Rectangle<float> (8.0f, 9.0f, 3.0f, 10.0f), 1.5f);
    g.setColour (Colours::text);
    g.setFont (FerriteLookAndFeel::font (12.0f, true));
    g.drawText (title.toUpperCase(), juce::Rectangle<int> (16, 4, getWidth() - 24, 20), juce::Justification::centredLeft);
}

void layoutRow (juce::Rectangle<int> area, std::initializer_list<juce::Component*> items, int gap)
{
    const int n = (int) items.size();
    if (n == 0) return;
    const int w = (area.getWidth() - gap * (n - 1)) / n;
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
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (Colours::bg);
    g.fillRoundedRectangle (r, 4.0f);

    // время каждого этапа в «визуальных» единицах (корень — чтобы короткие этапы были видны)
    auto vis = [] (float t) { return std::sqrt (juce::jmax (0.0f, t)) + 0.02f; };
    const float d = vis (vals[0]), a = vis (vals[1]), h = vis (vals[2]), dec = vis (vals[3]), rel = vis (vals[5]);
    const float sus = vals[4];
    const float susLen = 0.6f;
    const float total = d + a + h + dec + susLen + rel;
    auto X = [&] (float t) { return r.getX() + 4 + (r.getWidth() - 8) * t / total; };
    auto Y = [&] (float v) { return r.getBottom() - 4 - (r.getHeight() - 8) * v; };

    juce::Path p;
    float t = 0;
    p.startNewSubPath (X (0), Y (0));
    t += d;   p.lineTo (X (t), Y (0));
    t += a;   p.lineTo (X (t), Y (1));
    t += h;   p.lineTo (X (t), Y (1));
    const float t0 = t;
    for (int i = 1; i <= 16; ++i)
    {
        const float k = (float) i / 16.0f;
        p.lineTo (X (t0 + dec * k), Y (sus + (1 - sus) * std::exp (-5.0f * k)));
    }
    t += dec;
    t += susLen; p.lineTo (X (t), Y (sus));
    const float t1 = t;
    for (int i = 1; i <= 16; ++i)
    {
        const float k = (float) i / 16.0f;
        p.lineTo (X (t1 + rel * k), Y (sus * std::exp (-5.0f * k)));
    }

    juce::Path fill (p);
    fill.lineTo (X (total), Y (0));
    fill.closeSubPath();
    g.setColour (colour.withAlpha (0.15f));
    g.fillPath (fill);
    g.setColour (colour);
    g.strokePath (p, juce::PathStrokeType (1.6f));
}

void ScopeView::push (const float* data, int n)
{
    for (int i = 0; i < n; ++i)
    {
        buf[(size_t) write] = data[i];
        write = (write + 1) % (int) buf.size();
    }
}

void ScopeView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::bg);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (Colours::outline);
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);

    // триггер по переходу через ноль
    const int N = (int) buf.size();
    const int show = 512;
    int startIdx = (write - show - 256 + N * 2) % N;
    for (int i = 0; i < 256; ++i)
    {
        const int a = (startIdx + i) % N, b = (startIdx + i + 1) % N;
        if (buf[(size_t) a] <= 0.0f && buf[(size_t) b] > 0.0f) { startIdx = b; break; }
    }

    juce::Path p;
    for (int i = 0; i < show; ++i)
    {
        const float v = juce::jlimit (-1.0f, 1.0f, buf[(size_t) ((startIdx + i) % N)]);
        const float x = r.getX() + r.getWidth() * (float) i / (float) (show - 1);
        const float y = r.getCentreY() - v * r.getHeight() * 0.45f;
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    g.setColour (Colours::accent2);
    g.strokePath (p, juce::PathStrokeType (1.3f));
}
} // namespace ferrite
