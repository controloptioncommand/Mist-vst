#include "ModulationPanel.h"

namespace ferrite
{
FilterPanel::FilterPanel (FerriteProcessor& p) : Section ("Filter / Mod Env", Colours::accent2)
{
    auto& s = p.apvts;
    on.attach (s, "filterOn");
    type.attach (s, "filterType");
    cutoff.attach (s, "cutoff"); reso.attach (s, "reso"); env.attach (s, "filterEnv");
    key.attach (s, "filterKey"); drive.attach (s, "drive");
    ma.attach (s, "menv_attack"); md.attach (s, "menv_decay"); ms.attach (s, "menv_sustain"); mr.attach (s, "menv_release");
    for (juce::Component* c : std::initializer_list<juce::Component*> { &on, &type, &cutoff, &reso, &env, &key, &drive, &ma, &md, &ms, &mr, &modEnvLabel })
        addAndMakeVisible (c);
    for (auto* k : { &cutoff, &reso, &env, &key, &drive, &ma, &md, &ms, &mr })
        k->slider.getProperties().set ("accent", Colours::accent2.toString());

    modEnvLabel.setText ("MOD ENV", juce::dontSendNotification);
    modEnvLabel.setFont (FerriteLookAndFeel::font (10.0f, true));
    modEnvLabel.setColour (juce::Label::textColourId, Colours::textDim);
}

void FilterPanel::resized()
{
    auto r = getContentBounds();
    auto row1 = r.removeFromTop (r.getHeight() / 2);
    auto ctl = row1.removeFromLeft (80);
    on.setBounds (ctl.removeFromTop (22));
    type.setBounds (ctl.removeFromTop (40));
    layoutRow (row1, { &cutoff, &reso, &env, &key, &drive });
    r.removeFromTop (4);
    modEnvLabel.setBounds (r.removeFromLeft (80).withSizeKeepingCentre (80, 16));
    layoutRow (r, { &ma, &md, &ms, &mr });
}

// ===========================================================================
LfoPanel::LfoPanel (FerriteProcessor& p, int i) : Section ("LFO " + juce::String (i + 1), Colours::accent2), proc (p), index (i)
{
    auto& s = p.apvts;
    shape.attach (s, lfoId (i, "shape"));
    div.attach (s, lfoId (i, "div"));
    sync.attach (s, lfoId (i, "sync"));
    rate.attach (s, lfoId (i, "rate"));
    rate.slider.getProperties().set ("accent", Colours::accent2.toString());
    for (juce::Component* c : std::initializer_list<juce::Component*> { &shape, &div, &sync, &rate })
        addAndMakeVisible (c);
}

void LfoPanel::refresh()
{
    const bool synced = sync.getToggleState();
    rate.setEnabled (! synced);
    div.setEnabled (synced);
    const float v = proc.lfoDisplay[index].load();
    if (std::abs (v - shown) > 0.005f) { shown = v; repaint (meter); }
}

void LfoPanel::paintOverChildren (juce::Graphics& g)
{
    auto m = meter.toFloat();
    g.setColour (Colours::bg);
    g.fillRoundedRectangle (m, 3.0f);
    const float c = m.getCentreY();
    const float y = c - shown * m.getHeight() * 0.5f;
    g.setColour (Colours::accent2);
    g.fillRect (juce::Rectangle<float> (m.getX() + 2, juce::jmin (c, y), m.getWidth() - 4, std::abs (y - c) + 1));
}

void LfoPanel::resized()
{
    auto r = getContentBounds();
    meter = r.removeFromRight (10);
    r.removeFromRight (6);
    auto left = r.removeFromLeft (r.getWidth() / 2);
    shape.setBounds (left.removeFromTop (40));
    left.removeFromTop (4);
    sync.setBounds (left.removeFromTop (22));
    div.setBounds (left.removeFromTop (40));
    rate.setBounds (r.reduced (4, 8));
}

// ===========================================================================
ModMatrixPanel::ModMatrixPanel (FerriteProcessor& p) : Section ("Mod Matrix", Colours::accent2)
{
    auto& s = p.apvts;
    for (int i = 0; i < kNumModSlots; ++i)
    {
        auto& row = rows[(size_t) i];
        row.src.addItemList (modSourceNames(), 1);
        row.dst.addItemList (modDestNames(), 1);
        row.amt.setSliderStyle (juce::Slider::LinearHorizontal);
        row.amt.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 18);
        row.amt.setDoubleClickReturnValue (true, 0.0);
        row.amt.getProperties().set ("accent", Colours::accent2.toString());
        row.srcA = std::make_unique<APVTS::ComboBoxAttachment> (s, modId (i, "src"), row.src);
        row.dstA = std::make_unique<APVTS::ComboBoxAttachment> (s, modId (i, "dst"), row.dst);
        row.amtA = std::make_unique<APVTS::SliderAttachment> (s, modId (i, "amt"), row.amt);
        addAndMakeVisible (row.src);
        addAndMakeVisible (row.dst);
        addAndMakeVisible (row.amt);
    }
}

void ModMatrixPanel::resized()
{
    auto r = getContentBounds();
    const int h = r.getHeight() / kNumModSlots;
    for (auto& row : rows)
    {
        auto line = r.removeFromTop (h).reduced (0, 1);
        const int w = line.getWidth();
        row.src.setBounds (line.removeFromLeft (w * 30 / 100));
        line.removeFromLeft (4);
        row.dst.setBounds (line.removeFromLeft (w * 34 / 100));
        line.removeFromLeft (4);
        row.amt.setBounds (line);
    }
}
} // namespace ferrite
