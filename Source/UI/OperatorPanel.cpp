#include "OperatorPanel.h"

namespace ferrite
{
static bool isAudioFile (const juce::String& path)
{
    return juce::File (path).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg;caf;m4a");
}

// ===========================================================================
juce::RangedAudioParameter* WaveView::paramPtr (const char* name) const
{
    return proc.apvts.getParameter (opId (opIndex, name));
}

float WaveView::param (const char* name) const
{
    auto* p = paramPtr (name);
    return p != nullptr ? p->convertFrom0to1 (p->getValue()) : 0.0f;
}

void WaveView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::bg);
    g.fillRoundedRectangle (r, 4.0f);
    const auto col = Colours::forOperator (opIndex);
    auto asset = proc.getSample (opIndex);
    const bool sampleMode = (int) param ("mode") == OpModeSample;

    if (asset != nullptr && sampleMode)
    {
        const int bins = (int) asset->overview.size() / 2;
        const float mid = r.getCentreY(), hh = r.getHeight() * 0.45f;
        g.setColour (col.withAlpha (0.8f));
        for (int x = 0; x < (int) r.getWidth(); ++x)
        {
            const int b = juce::jlimit (0, bins - 1, (int) ((float) x / r.getWidth() * (float) bins));
            const float mn = asset->overview[(size_t) b * 2], mx = asset->overview[(size_t) b * 2 + 1];
            g.drawVerticalLine ((int) r.getX() + x, mid - mx * hh, mid - mn * hh + 1.0f);
        }

        const float ls = param ("loopStart"), le = param ("loopEnd"), st = param ("start");
        const int loopMode = (int) param ("loopMode");
        if (loopMode != LoopOff)
        {
            g.setColour (Colours::accent2.withAlpha (0.12f));
            g.fillRect (juce::Rectangle<float> (r.getX() + ls * r.getWidth(), r.getY(), (le - ls) * r.getWidth(), r.getHeight()));
            g.setColour (Colours::accent2);
            g.drawVerticalLine ((int) (r.getX() + ls * r.getWidth()), r.getY(), r.getBottom());
            g.drawVerticalLine ((int) (r.getX() + le * r.getWidth()) - 1, r.getY(), r.getBottom());
        }
        g.setColour (Colours::text);
        g.drawVerticalLine ((int) (r.getX() + st * r.getWidth()), r.getY(), r.getBottom());
        if (! compact)
        {
            g.setFont (FerriteLookAndFeel::font (10.0f));
            g.drawText ("S", juce::Rectangle<float> (r.getX() + st * r.getWidth() + 2, r.getY() + 2, 12, 12), juce::Justification::centredLeft);
        }
    }
    else
    {
        // текущий кадр wavetable
        const Wavetable& t = asset ? asset->table : Wavetable::getBasic();
        const float scanV = param ("scan");
        juce::Path p;
        const int W = juce::jmax (2, (int) r.getWidth());
        for (int x = 0; x < W; ++x)
        {
            const float v = t.read ((double) x / (W - 1) * 0.999, scanV, 0);
            const float y = r.getCentreY() - v * r.getHeight() * 0.42f;
            if (x == 0) p.startNewSubPath (r.getX() + (float) x, y); else p.lineTo (r.getX() + (float) x, y);
        }
        g.setColour (col.withAlpha (0.25f));
        g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());
        g.setColour (col);
        g.strokePath (p, juce::PathStrokeType (compact ? 1.2f : 1.8f));
    }

    if (! compact && asset == nullptr)
    {
        g.setColour (Colours::textDim);
        g.setFont (FerriteLookAndFeel::font (11.0f));
        g.drawText ("drop audio file here", r.reduced (6), juce::Justification::bottomRight);
    }
}

void WaveView::mouseDown (const juce::MouseEvent& e)
{
    dragging = nullptr;
    if (compact || proc.getSample (opIndex) == nullptr || (int) param ("mode") != OpModeSample)
    {
        if (auto* parent = getParentComponent()) parent->mouseDown (e.getEventRelativeTo (parent));
        return;
    }
    const float x = (float) e.x / (float) getWidth();
    const char* names[] = { "start", "loopStart", "loopEnd" };
    float best = 0.05f;
    for (auto* n : names)
    {
        const float d = std::abs (param (n) - x);
        if (d < best) { best = d; dragging = paramPtr (n); }
    }
    if (dragging) dragging->beginChangeGesture();
}

void WaveView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging == nullptr) return;
    const float x = juce::jlimit (0.0f, 1.0f, (float) e.x / (float) getWidth());
    dragging->setValueNotifyingHost (dragging->convertTo0to1 (x));
    repaint();
}

void WaveView::mouseUp (const juce::MouseEvent&)
{
    if (dragging) dragging->endChangeGesture();
    dragging = nullptr;
}

// ===========================================================================
OpCard::OpCard (FerriteProcessor& p, int o, std::function<void (int)> sel)
    : proc (p), op (o), onSelect (std::move (sel)), level ("", Colours::forOperator (o)), wave (p)
{
    onButton.attach (proc.apvts, opId (op, "on"));
    addAndMakeVisible (onButton);
    level.attach (proc.apvts, opId (op, "level"));
    level.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (level);

    nameLabel.setText ("OP " + juce::String (op + 1), juce::dontSendNotification);
    nameLabel.setFont (FerriteLookAndFeel::font (13.0f, true));
    nameLabel.setColour (juce::Label::textColourId, Colours::forOperator (op));
    nameLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (nameLabel);

    ratioLabel.setFont (FerriteLookAndFeel::font (11.0f));
    ratioLabel.setColour (juce::Label::textColourId, Colours::textDim);
    ratioLabel.setJustificationType (juce::Justification::centredRight);
    ratioLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (ratioLabel);

    wave.setOperator (op);
    wave.setCompact (true);
    wave.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (wave);
}

void OpCard::refresh()
{
    auto get = [&] (const char* n) { auto* pp = proc.apvts.getParameter (opId (op, n)); return pp->convertFrom0to1 (pp->getValue()); };
    juce::String txt;
    if (get ("fixed") > 0.5f) txt = juce::String (get ("fixedHz"), 1) + " Hz";
    else txt = "x" + juce::String (coarseToRatio ((int) get ("coarse")) * (1.0f + get ("fine")), 2);
    if (auto a = proc.getSample (op)) txt = a->name.substring (0, 12) + "  " + txt;
    ratioLabel.setText (txt, juce::dontSendNotification);
    wave.repaint();
}

void OpCard::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (selected ? Colours::panelHi : Colours::panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (selected ? Colours::forOperator (op) : Colours::outline);
    g.drawRoundedRectangle (r, 6.0f, selected ? 1.5f : 1.0f);
}

void OpCard::paintOverChildren (juce::Graphics& g)
{
    if (dragOver)
    {
        g.setColour (Colours::forOperator (op).withAlpha (0.2f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
        g.setColour (Colours::forOperator (op));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 6.0f, 2.0f);
    }
}

void OpCard::resized()
{
    auto r = getLocalBounds().reduced (6);
    auto top = r.removeFromTop (20);
    onButton.setBounds (top.removeFromLeft (20));
    nameLabel.setBounds (top.removeFromLeft (44));
    ratioLabel.setBounds (top);
    r.removeFromTop (4);
    level.setBounds (r.removeFromRight (r.getHeight()).reduced (2));
    r.removeFromRight (4);
    wave.setBounds (r);
}

void OpCard::mouseDown (const juce::MouseEvent&) { if (onSelect) onSelect (op); }

bool OpCard::isInterestedInFileDrag (const juce::StringArray& files)
{
    return files.size() == 1 && isAudioFile (files[0]);
}

void OpCard::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;
    repaint();
    if (files.size() > 0)
    {
        proc.loadSampleAsync (op, juce::File (files[0]));
        if (onSelect) onSelect (op);
    }
}

// ===========================================================================
OperatorDetail::OperatorDetail (FerriteProcessor& p) : Section ("Operator 1"), proc (p), wave (p)
{
    for (auto* k : knobs()) addAndMakeVisible (k);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &mode, &fixed, &envView, &wave, &loopMode, &reverse, &keytrack,
                                                                        &loadButton, &clearButton, &sampleInfo })
        addAndMakeVisible (c);

    sampleInfo.setFont (FerriteLookAndFeel::font (11.0f));
    sampleInfo.setColour (juce::Label::textColourId, Colours::textDim);

    clearButton.onClick = [this] { proc.clearSample (op); };
    loadButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Load sample into Op " + juce::String (op + 1),
                                                       juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                       "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg;*.caf;*.m4a");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  auto f = fc.getResult();
                                  if (f.existsAsFile()) proc.loadSampleAsync (op, f);
                              });
    };
    setOperator (0);
}

std::vector<ParamKnob*> OperatorDetail::knobs()
{
    return { &coarse, &fine, &detune, &fixedHz, &level, &velSens, &feedback, &scan, &pan, &out,
             &delay, &attack, &hold, &decay, &sustain, &release, &start, &loopStart, &loopEnd, &xfade, &root };
}

void OperatorDetail::setOperator (int newOp)
{
    op = newOp;
    const auto col = Colours::forOperator (op);
    setTitle ("Operator " + juce::String (op + 1));
    setAccent (col);
    auto& s = proc.apvts;
    mode.attach (s, opId (op, "mode"));
    coarse.attach (s, opId (op, "coarse"));     fine.attach (s, opId (op, "fine"));
    detune.attach (s, opId (op, "detune"));     fixedHz.attach (s, opId (op, "fixedHz"));
    fixed.attach (s, opId (op, "fixed"));
    level.attach (s, opId (op, "level"));       velSens.attach (s, opId (op, "velSens"));
    feedback.attach (s, opId (op, "feedback")); scan.attach (s, opId (op, "scan"));
    pan.attach (s, opId (op, "pan"));           out.attach (s, opId (op, "out"));
    delay.attach (s, opId (op, "delay"));       attack.attach (s, opId (op, "attack"));
    hold.attach (s, opId (op, "hold"));         decay.attach (s, opId (op, "decay"));
    sustain.attach (s, opId (op, "sustain"));   release.attach (s, opId (op, "release"));
    start.attach (s, opId (op, "start"));       loopStart.attach (s, opId (op, "loopStart"));
    loopEnd.attach (s, opId (op, "loopEnd"));   xfade.attach (s, opId (op, "xfade"));
    root.attach (s, opId (op, "root"));
    loopMode.attach (s, opId (op, "loopMode"));
    reverse.attach (s, opId (op, "reverse"));   keytrack.attach (s, opId (op, "keytrack"));

    for (auto* k : knobs()) k->slider.getProperties().set ("accent", col.toString());
    envView.setColour (col);
    wave.setOperator (op);
    refresh();
}

void OperatorDetail::refresh()
{
    auto get = [&] (const char* n) { auto* pp = proc.apvts.getParameter (opId (op, n)); return pp->convertFrom0to1 (pp->getValue()); };
    envView.setValues (get ("delay"), get ("attack"), get ("hold"), get ("decay"), get ("sustain"), get ("release"));
    const bool isFixed = get ("fixed") > 0.5f;
    coarse.setEnabled (! isFixed);
    fine.setEnabled (! isFixed);
    fixedHz.setEnabled (isFixed);

    auto asset = proc.getSample (op);
    const bool sampleMode = (int) get ("mode") == OpModeSample;
    for (juce::Component* c : std::initializer_list<juce::Component*> { &start, &loopStart, &loopEnd, &xfade, &loopMode, &reverse })
        c->setEnabled (sampleMode && asset != nullptr);
    clearButton.setEnabled (asset != nullptr);

    juce::String info;
    if (asset)
    {
        info << asset->name << "  |  " << juce::String ((double) asset->length / asset->fileSampleRate, 2) << " s";
        if (asset->detectedMidi > 0)
            info << "  |  detected " << juce::MidiMessage::getMidiNoteName ((int) std::lround (asset->detectedMidi), true, true, 4)
                 << " (" << juce::String ((asset->detectedMidi - std::round (asset->detectedMidi)) * 100.0f, 0) << " ct)";
        else
            info << "  |  no pitch";
        info << "  |  " << asset->table.getNumFrames() << " frames";
    }
    else
    {
        const auto err = proc.getLastError();
        info = err.isNotEmpty() ? err : "Built-in wavetable: Sine > Sine+Oct > Tri > Square > Saw > Pulse > Narrow > Formant";
    }
    sampleInfo.setText (info, juce::dontSendNotification);
    wave.repaint();
}

void OperatorDetail::resized()
{
    auto r = getContentBounds();
    const int kh = 62;

    // ряд 1: тон
    auto row1 = r.removeFromTop (kh);
    mode.setBounds (row1.removeFromLeft (90).withTrimmedTop (4).withTrimmedBottom (14));
    row1.removeFromLeft (6);
    fixed.setBounds (row1.removeFromLeft (64).withSizeKeepingCentre (64, 22));
    layoutRow (row1, { &coarse, &fine, &detune, &fixedHz, &level, &velSens, &feedback, &scan, &pan, &out });

    r.removeFromTop (6);
    // ряд 2: огибающая + график
    auto row2 = r.removeFromTop (kh);
    envView.setBounds (row2.removeFromRight (220).reduced (2));
    row2.removeFromRight (6);
    layoutRow (row2, { &delay, &attack, &hold, &decay, &sustain, &release });

    r.removeFromTop (6);
    // ряд 3: семпл
    auto info = r.removeFromBottom (18);
    auto buttons = info.removeFromRight (130);
    clearButton.setBounds (buttons.removeFromRight (60));
    buttons.removeFromRight (6);
    loadButton.setBounds (buttons);
    sampleInfo.setBounds (info);
    r.removeFromBottom (4);

    auto controls = r.removeFromRight (440);
    wave.setBounds (r.reduced (0, 2));
    controls.removeFromLeft (8);
    auto toggles = controls.removeFromRight (90);
    loopMode.setBounds (toggles.removeFromTop (40));
    reverse.setBounds (toggles.removeFromTop (22));
    keytrack.setBounds (toggles.removeFromTop (22));
    layoutRow (controls.withSizeKeepingCentre (controls.getWidth(), juce::jmin (controls.getHeight(), kh)),
               { &start, &loopStart, &loopEnd, &xfade, &root });
}

bool OperatorDetail::isInterestedInFileDrag (const juce::StringArray& files)
{
    return files.size() == 1 && isAudioFile (files[0]);
}

void OperatorDetail::filesDropped (const juce::StringArray& files, int, int)
{
    if (files.size() > 0) proc.loadSampleAsync (op, juce::File (files[0]));
}
} // namespace ferrite
