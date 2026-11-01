#include "ModulationPanel.h"

namespace mist
{
FilterPanel::FilterPanel (MistProcessor& p) : Section ("Filter / Mod Env")
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
        k->slider.getProperties().set ("accent", "accent");

    modEnvLabel.setText ("MOD ENV", juce::dontSendNotification);
    modEnvLabel.setFont (MistLookAndFeel::font (10.0f, true));
    styleLabel (modEnvLabel, true);
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
LfoPanel::LfoPanel (MistProcessor& p, int i) : Section ("LFO " + juce::String (i + 1), Colours::accent2), proc (p), index (i)
{
    auto& s = p.apvts;
    shape.attach (s, lfoId (i, "shape"));
    div.attach (s, lfoId (i, "div"));
    sync.attach (s, lfoId (i, "sync"));
    rate.attach (s, lfoId (i, "rate"));
    rate.slider.getProperties().set ("accent", "accent");
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
    g.setColour (Colours::panelHi);
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
ModMatrixPanel::ModMatrixPanel (MistProcessor& p) : Section ("Mod Matrix")
{
    auto& s = p.apvts;
    for (int i = 0; i < kNumModSlots; ++i)
    {
        auto& row = rows[(size_t) i];
        row.src.addItemList (modSourceNames(), 1);
        row.dst.addItemList (modDestNames(), 1);
        row.amt.setSliderStyle (juce::Slider::LinearHorizontal);
        row.amt.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        row.value.setFont (MistLookAndFeel::mono (10.5f));
        styleLabel (row.value, true);
        row.value.setJustificationType (juce::Justification::centredRight);
        auto* rowPtr = &row;
        row.amt.onValueChange = [rowPtr] { rowPtr->value.setText (rowPtr->amt.getTextFromValue (rowPtr->amt.getValue()), juce::dontSendNotification); };
        row.amt.setDoubleClickReturnValue (true, 0.0);
        row.amt.getProperties().set ("accent", "accent");
        row.srcA = std::make_unique<APVTS::ComboBoxAttachment> (s, modId (i, "src"), row.src);
        row.dstA = std::make_unique<APVTS::ComboBoxAttachment> (s, modId (i, "dst"), row.dst);
        row.amtA = std::make_unique<APVTS::SliderAttachment> (s, modId (i, "amt"), row.amt);
        addAndMakeVisible (row.src);
        addAndMakeVisible (row.dst);
        addAndMakeVisible (row.amt);
        addAndMakeVisible (row.value);
        row.amt.onValueChange();
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
        row.value.setBounds (line.removeFromRight (48));
        row.amt.setBounds (line.reduced (6, 0));
    }
}
} // namespace mist

namespace mist
{
VoicePage::VoicePage (MistProcessor& p)
{
    auto& s = p.apvts;
    polyMode.attach (s, "polyMode");
    glide.attach (s, "glide"); bend.attach (s, "bendRange"); uni.attach (s, "uniVoices");
    uniDetune.attach (s, "uniDetune"); uniSpread.attach (s, "uniSpread");
    fm.attach (s, "fmAmount"); master.attach (s, "master");
    for (juce::Component* c : std::initializer_list<juce::Component*> { &polyMode, &glide, &bend, &uni, &uniDetune, &uniSpread, &fm, &master })
        addAndMakeVisible (c);
}

void VoicePage::resized()
{
    auto r = getLocalBounds();
    auto row1 = r.removeFromTop (92);
    polyMode.setBounds (row1.removeFromLeft (130).withSizeKeepingCentre (130, 44));
    row1.removeFromLeft (12);
    layoutRow (row1, std::vector<juce::Component*> { &glide, &bend, &fm, &master }, 4, 96);
    r.removeFromTop (8);
    auto row2 = r.removeFromTop (92);
    row2.removeFromLeft (142);
    layoutRow (row2, std::vector<juce::Component*> { &uni, &uniDetune, &uniSpread }, 4, 96);
}
} // namespace mist
