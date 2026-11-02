#include "Widgets.h"
#include "../DSP/Primitives.h"

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

EnvelopeView::EnvelopeView()
{
    setWantsKeyboardFocus (true);
    setTitle ("Envelope shape");
}

void EnvelopeView::attach (APVTS& s, const Ids& newIds)
{
    state = &s;
    ids = newIds;
    refresh();
}

float EnvelopeView::value (const juce::String& id) const
{
    if (state == nullptr || id.isEmpty()) return 0.0f;
    auto* p = state->getParameter (id);
    return p != nullptr ? p->convertFrom0to1 (p->getValue()) : 0.0f;
}

juce::RangedAudioParameter* EnvelopeView::curveParam (int seg) const
{
    if (state == nullptr) return nullptr;
    return state->getParameter (seg == 0 ? ids.aCurve : seg == 1 ? ids.dCurve : ids.rCurve);
}

void EnvelopeView::refresh()
{
    const std::array<float, 9> v { value (ids.delay), value (ids.attack), value (ids.hold), value (ids.decay), value (ids.sustain),
                                   value (ids.release), value (ids.aCurve), value (ids.dCurve), value (ids.rCurve) };
    if (v != vals)
    {
        vals = v;
        updateDescription();
        repaint();
    }
}

void EnvelopeView::updateDescription()
{
    auto pct = [] (float c) { const int p = juce::roundToInt (c * 100.0f); return (p > 0 ? "+" : "") + juce::String (p) + "%"; };
    static const char* names[] = { "Attack", "Decay", "Release" };
    setDescription (juce::String ("Curves: attack ") + pct (vals[6]) + ", decay " + pct (vals[7]) + ", release " + pct (vals[8])
                    + ". Selected: " + names[focused] + ". Left and right arrows select a segment, up and down bend it.");
}

void EnvelopeView::paint (juce::Graphics& g)
{
    using LnF = MistLookAndFeel;
    const auto colour = hasColour ? this->colour : Colours::accent;
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

    EnvSegment segA, segD, segR;
    segA.setCurve (vals[6]); segD.setCurve (vals[7]); segR.setCurve (vals[8]);
    constexpr int steps = 32;

    juce::Path p;
    float t = 0;
    p.startNewSubPath (X (0), Y (0));
    t += d;   p.lineTo (X (t), Y (0));
    const float tA = t;
    for (int i = 1; i <= steps; ++i)
    {
        const float k = (float) i / steps;
        p.lineTo (X (tA + a * k), Y ((float) segA.shape (k)));
    }
    handles[0] = { X (tA + a * 0.5f), Y ((float) segA.shape (0.5)) };
    t += a;
    t += h;   p.lineTo (X (t), Y (1));
    const float t0 = t;
    for (int i = 1; i <= steps; ++i)
    {
        const float k = (float) i / steps;
        p.lineTo (X (t0 + dec * k), Y (1.0f - (1.0f - sus) * (float) segD.shape (k)));
    }
    handles[1] = { X (t0 + dec * 0.5f), Y (1.0f - (1.0f - sus) * (float) segD.shape (0.5)) };
    t += dec;
    t += susLen; p.lineTo (X (t), Y (sus));
    const float t1 = t;
    for (int i = 1; i <= steps; ++i)
    {
        const float k = (float) i / steps;
        p.lineTo (X (t1 + rel * k), Y (sus * (1.0f - (float) segR.shape (k))));
    }
    handles[2] = { X (t1 + rel * 0.5f), Y (sus * (1.0f - (float) segR.shape (0.5))) };

    juce::Path fill (p);
    fill.lineTo (X (total), Y (0));
    fill.closeSubPath();
    g.setColour (colour.withAlpha (0.10f));
    g.fillPath (fill);
    g.setColour (colour);
    g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // точки изгиба
    const bool kbd = hasKeyboardFocus (false);
    for (int i = 0; i < 3; ++i)
    {
        const bool active = i == hover || i == dragging || (kbd && i == focused);
        const float rad = active ? 5.5f : 4.5f;
        auto c = juce::Rectangle<float> (rad * 2, rad * 2).withCentre (handles[(size_t) i]);
        g.setColour (active ? colour : Colours::surface);
        g.fillEllipse (c);
        g.setColour (colour);
        g.drawEllipse (c, 1.5f);
    }

    // во время перетаскивания — значение изгиба
    const int shown = dragging >= 0 ? dragging : hover;
    if (shown >= 0)
    {
        const int pctv = juce::roundToInt (vals[(size_t) (6 + shown)] * 100.0f);
        g.setFont (LnF::mono (LnF::Type::footnote));
        g.setColour (Colours::ink2);
        static const char* names[] = { "Attack curve ", "Decay curve ", "Release curve " };
        g.drawText (names[shown] + juce::String (pctv > 0 ? "+" : "") + juce::String (pctv) + "%",
                    r.reduced (10.0f, 6.0f).removeFromTop (14.0f), juce::Justification::centredRight);
    }
    if (kbd) LnF::drawFocusRing (g, r, 8.0f);
}

int EnvelopeView::handleAt (juce::Point<float> pt) const
{
    int best = -1;
    float bestD = 12.0f;   // цель нажатия ≥ 24 px
    for (int i = 0; i < 3; ++i)
    {
        const float dd = pt.getDistanceFrom (handles[(size_t) i]);
        if (dd < bestD) { bestD = dd; best = i; }
    }
    return best;
}

void EnvelopeView::mouseMove (const juce::MouseEvent& e)
{
    const int h = handleAt (e.position);
    setHover (h);
    setMouseCursor (h >= 0 ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
}

void EnvelopeView::mouseDown (const juce::MouseEvent& e)
{
    dragging = handleAt (e.position);
    if (dragging < 0) return;
    focused = dragging;
    if (auto* p = curveParam (dragging))
    {
        dragStartCurve = p->convertFrom0to1 (p->getValue());
        p->beginChangeGesture();
    }
    repaint();
}

void EnvelopeView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0) return;
    if (auto* p = curveParam (dragging))
    {
        // атака растёт — тянем вверх, кривая выгибается вверх (+); у спада и релиза наоборот
        const float sign = dragging == 0 ? 1.0f : -1.0f;
        const float sens = e.mods.isShiftDown() ? 600.0f : 150.0f;
        const float v = juce::jlimit (-1.0f, 1.0f, dragStartCurve - sign * (float) e.getDistanceFromDragStartY() / sens);
        p->setValueNotifyingHost (p->convertTo0to1 (v));
        refresh();
    }
}

void EnvelopeView::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0)
        if (auto* p = curveParam (dragging)) p->endChangeGesture();
    dragging = -1;
    repaint();
}

void EnvelopeView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int h = handleAt (e.position);
    if (h < 0) return;
    if (auto* p = curveParam (h))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->getDefaultValue());
        p->endChangeGesture();
        refresh();
    }
}

void EnvelopeView::nudgeCurve (int seg, float delta)
{
    if (auto* p = curveParam (seg))
    {
        const float v = juce::jlimit (-1.0f, 1.0f, p->convertFrom0to1 (p->getValue()) + delta);
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (v));
        p->endChangeGesture();
        refresh();
    }
}

bool EnvelopeView::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::leftKey)  { focused = (focused + 2) % 3; updateDescription(); repaint(); return true; }
    if (k == juce::KeyPress::rightKey) { focused = (focused + 1) % 3; updateDescription(); repaint(); return true; }
    // ↑ — точка вверх (у атаки изгиб +, у спада и релиза −)
    const float sign = focused == 0 ? 1.0f : -1.0f;
    if (k == juce::KeyPress::upKey)   { nudgeCurve (focused, 0.05f * sign);  return true; }
    if (k == juce::KeyPress::downKey) { nudgeCurve (focused, -0.05f * sign); return true; }
    return false;
}
} // namespace mist
