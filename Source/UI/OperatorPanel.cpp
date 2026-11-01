#include "OperatorPanel.h"
#include "RoutingPanel.h"

namespace mist
{
static bool isAudioFile (const juce::String& path)
{
    return juce::File (path).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg;caf;m4a");
}

static float getParam (MistProcessor& p, const juce::String& id)
{
    auto* pp = p.apvts.getParameter (id);
    return pp != nullptr ? pp->convertFrom0to1 (pp->getValue()) : 0.0f;
}

juce::String operatorRole (MistProcessor& p, int op)
{
    const auto r = Routing::read (p);
    if (! r.on[op]) return "Off";
    if (r.out[op] > 0.0f) return "Carrier";
    for (int d = 0; d < kNumOps; ++d) if (r.mod[op][d] > 0.0f) return "Modulator";
    return "Unused";
}

juce::String modeName (int mode)
{
    switch (mode)
    {
        case OpModeSample: return "Sample PM";
        case OpModeGrain:  return "Grain";
        default:           return "Scrub";
    }
}

juce::String modeDescription (int mode)
{
    switch (mode)
    {
        case OpModeSample: return "Modulators bend this operator's read position in the sample, sample by sample.";
        case OpModeGrain:  return "Overlapping grains from the sample at Position; modulators bend every grain's read head.";
        default:           return "A wavetable sliced from the sample by its pitch period. Position scrubs through it.";
    }
}

// ===========================================================================
SampleSlot::SampleSlot (MistProcessor& p, int o, std::function<void (int)> sel)
    : proc (p), op (o), onSelect (std::move (sel))
{
    reverse.setClickingTogglesState (true);
    reverse.getProperties().set ("chipH", 22.0);
    reverse.setTitle ("Reverse, Op " + juce::String (op + 1));
    reverseAttachment = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, opId (op, "reverse"), reverse);
    addChildComponent (reverse);
    setWantsKeyboardFocus (true);
    setTitle ("Op " + juce::String (op + 1));
}

float SampleSlot::param (const char* name) const { return getParam (proc, opId (op, name)); }

bool SampleSlot::hasSample() const { return proc.getSample (op) != nullptr; }

void SampleSlot::refresh()
{
    const auto role = operatorRole (proc, op);
    const bool filled = hasSample();
    if (role != cachedRole || filled != reverse.isVisible())
    {
        cachedRole = role;
        reverse.setVisible (filled);
        resized();
    }
    auto asset = proc.getSample (op);
    setDescription ("Op " + juce::String (op + 1) + ", " + cachedRole + ", "
                    + (asset ? "sample " + asset->name : juce::String ("no sample, drop an audio file here")));
    repaint();
}

void SampleSlot::paint (juce::Graphics& g)
{
    using LnF = MistLookAndFeel;
    auto r = getLocalBounds().toFloat();
    const float radius = 12.0f, pad = 12.0f;
    const bool isOn = param ("on") > 0.5f;
    const auto roleCol = isOn ? Colours::forOperator (op) : Colours::ink2;

    // карточка плотная, без стекла и теней
    g.setColour (Colours::surface);
    g.fillRoundedRectangle (r, radius);
    g.setColour (selected ? Colours::ink : Colours::outline);   // выбранная карточка — рамка Ink
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);

    auto asset = proc.getSample (op);
    auto content = getLocalBounds().reduced ((int) pad, 8);

    // ● Op 1 · Carrier                    имя файла
    auto top = content.removeFromTop (18);
    powerArea = top.removeFromLeft (14).expanded (6, 6);
    g.setColour (roleCol);
    g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ (float) top.getX() - 7.0f, (float) top.getCentreY() }));
    g.setFont (LnF::font (LnF::Type::callout, true));
    const juce::String opText = "Op " + juce::String (op + 1) + midDot();
    const int opW = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), opText);
    g.setColour (isOn ? Colours::ink : Colours::ink2);
    g.drawText (opText, top.removeFromLeft (opW + 1), juce::Justification::centredLeft);
    g.setColour (roleCol);
    const int roleW = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), cachedRole) + 4;
    g.drawText (cachedRole, top.removeFromLeft (roleW), juce::Justification::centredLeft);
    g.setFont (LnF::font (LnF::Type::subheadline));
    g.setColour (Colours::ink2);
    const int mode = (int) param ("mode");
    g.drawText (asset ? asset->name : juce::String ("no sample"), top, juce::Justification::centredRight, true);

    if (asset == nullptr)
    {
        // пустая карточка: Scrub · built-in · ×1.00          Drop a sample
        auto row = content.removeFromTop (16);
        waveArea = getLocalBounds();
        g.setFont (LnF::mono (LnF::Type::footnote));
        g.setColour (Colours::ink2);
        const float ratio = coarseToRatio ((int) param ("coarse")) * (1.0f + param ("fine"));
        g.drawText (modeName (mode) + midDot() + "built-in" + midDot() + juce::String::fromUTF8 ("\xc3\x97") + juce::String (ratio, 2),
                    row, juce::Justification::centredLeft);
        g.setFont (LnF::font (LnF::Type::subheadline));
        g.drawText (dragOver ? "Release to load" : "Drop a sample", row, juce::Justification::centredRight);
    }
    else
    {
        content.removeFromTop (4);
        auto bottom = content.removeFromBottom (28);
        waveArea = content;
        auto wa = waveArea.toFloat();
        // колодец: радиус = радиус карточки − отступ
        g.setColour (Colours::well);
        g.fillRoundedRectangle (wa, juce::jmax (2.0f, radius - pad));

        auto plot = wa.reduced (6.0f, 4.0f).withTrimmedRight (28.0f);
        const float mid = plot.getCentreY(), hh = plot.getHeight() * 0.5f;
        const float dashes[] = { 2.0f, 3.0f };
        g.setFont (LnF::mono (LnF::Type::caption));
        for (float db : { -6.0f, -12.0f })
        {
            const float a = juce::Decibels::decibelsToGain (db) * hh;
            g.setColour (Colours::ink2.withAlpha (0.4f));
            g.drawDashedLine ({ plot.getX(), mid - a, plot.getRight(), mid - a }, dashes, 2, 1.0f);
            g.drawDashedLine ({ plot.getX(), mid + a, plot.getRight(), mid + a }, dashes, 2, 1.0f);
        }
        g.setColour (Colours::ink2);
        auto lab = wa.withTrimmedLeft (wa.getWidth() - 32.0f).reduced (2.0f, 2.0f);
        auto labelAt = [&] (const juce::String& t, float y)
        {
            g.drawText (t, juce::Rectangle<float> (lab.getX(), y - 5.5f, lab.getWidth(), 11.0f), juce::Justification::centredRight);
        };
        labelAt ("0 dB", juce::jmax (lab.getY() + 5.5f, mid - hh));
        labelAt (juce::String::fromUTF8 ("\xe2\x88\x92" "6"), mid - juce::Decibels::decibelsToGain (-6.0f) * hh);
        labelAt (juce::String::fromUTF8 ("\xe2\x88\x92" "12"), mid - juce::Decibels::decibelsToGain (-12.0f) * hh);

        // плотная волна: полоска на каждую колонку пикселей, цвет по громкости, 10 корзин
        const int bins = (int) asset->overview.size() / 2;
        const int W = juce::jmax (1, (int) plot.getWidth());
        juce::RectangleList<float> buckets[10];
        for (int x = 0; x < W && bins > 0; ++x)
        {
            const int b0 = x * bins / W, b1 = juce::jmax (b0 + 1, (x + 1) * bins / W);
            float mn = 0, mx = 0;
            for (int b = b0; b < juce::jmin (b1, bins); ++b)
            {
                mn = juce::jmin (mn, asset->overview[(size_t) b * 2]);
                mx = juce::jmax (mx, asset->overview[(size_t) b * 2 + 1]);
            }
            const float pk = juce::jmax (-mn, mx);
            const float y0 = mid - mx * hh, y1 = mid - mn * hh;
            buckets[rampBucket (juce::Decibels::gainToDecibels (pk, -100.0f))]
                .addWithoutMerging ({ plot.getX() + (float) x, y0, 1.0f, juce::jmax (1.0f, y1 - y0) });
        }
        const auto& ramp = Colours::rampForOperator (op);
        for (int b = 0; b < 10; ++b)
        {
            if (buckets[b].isEmpty()) continue;
            g.setColour (isOn ? rampBucketColour (ramp, b) : Colours::ink2);
            g.fillRectList (buckets[b]);
        }

        // Start / Length
        const double secs = (double) asset->length / asset->fileSampleRate;
        g.setFont (LnF::mono (LnF::Type::footnote));
        g.setColour (Colours::ink2);
        g.drawText ("Start " + juce::String (param ("start") * secs, 2) + " s   Length " + juce::String (secs, 2) + " s",
                    bottom.withTrimmedRight (80), juce::Justification::centredLeft);
    }

    if (dragOver)
    {
        g.setColour (Colours::signal);
        g.drawRoundedRectangle (r.reduced (1.0f), radius - 1.0f, 2.0f);
    }
    if (hasKeyboardFocus (false))
        LnF::drawFocusRing (g, r, radius);
}

void SampleSlot::resized()
{
    auto r = getLocalBounds().reduced (12, 8);
    reverse.setBounds (r.removeFromBottom (28).removeFromRight (76));
}

bool SampleSlot::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::returnKey || k == juce::KeyPress::spaceKey)
    {
        if (onSelect) onSelect (op);
        if (! hasSample()) openFileChooser();
        return true;
    }
    return false;
}

void SampleSlot::mouseDown (const juce::MouseEvent& e)
{
    if (powerArea.expanded (4).contains (e.getPosition()))
    {
        if (auto* p = proc.apvts.getParameter (opId (op, "on")))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
            p->endChangeGesture();
        }
        return;
    }
    if (onSelect) onSelect (op);

    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu m;
        m.addItem (1, "Load sample...");
        m.addItem (2, "Clear sample", proc.getSample (op) != nullptr);
        m.addSeparator();
        for (int md : { OpModeSample, OpModeWave, OpModeGrain })
            m.addItem (10 + md, modeName (md), true, (int) param ("mode") == md);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this] (int res)
        {
            if (res == 1) openFileChooser();
            else if (res == 2) proc.clearSample (op);
            else if (res >= 10)
                if (auto* p = proc.apvts.getParameter (opId (op, "mode")))
                {
                    p->beginChangeGesture();
                    p->setValueNotifyingHost (p->convertTo0to1 ((float) (res - 10)));
                    p->endChangeGesture();
                }
        });
    }
}

void SampleSlot::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (waveArea.contains (e.getPosition())) openFileChooser();
}

void SampleSlot::openFileChooser()
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
}

bool SampleSlot::isInterestedInFileDrag (const juce::StringArray& files)
{
    return files.size() == 1 && isAudioFile (files[0]);
}

void SampleSlot::filesDropped (const juce::StringArray& files, int, int)
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
juce::RangedAudioParameter* WaveView::paramPtr (const char* name) const
{
    return proc.apvts.getParameter (opId (opIndex, name));
}

float WaveView::param (const char* name) const { return getParam (proc, opId (opIndex, name)); }

void WaveView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::well);
    g.fillRoundedRectangle (r, 8.0f);
    const auto col = Colours::forOperator (opIndex);
    auto asset = proc.getSample (opIndex);
    const int mode = (int) param ("mode");
    auto inner = r.reduced (8.0f, 6.0f);

    if (asset != nullptr && mode != OpModeWave)
    {
        const int bins = (int) asset->overview.size() / 2;
        const float mid = inner.getCentreY(), hh = inner.getHeight() * 0.48f;
        g.setColour (col.withAlpha (0.85f));
        for (int x = 0; x < (int) inner.getWidth(); ++x)
        {
            const int b = juce::jlimit (0, bins - 1, (int) ((float) x / inner.getWidth() * (float) bins));
            const float mn = asset->overview[(size_t) b * 2], mx = asset->overview[(size_t) b * 2 + 1];
            g.drawVerticalLine ((int) inner.getX() + x, mid - mx * hh, mid - mn * hh + 1.0f);
        }

        auto X = [&] (float v) { return inner.getX() + v * inner.getWidth(); };
        if (mode == OpModeSample)
        {
            const float ls = param ("loopStart"), le = param ("loopEnd"), st = param ("start");
            if ((int) param ("loopMode") != LoopOff)
            {
                g.setColour (Colours::accent.withAlpha (0.10f));
                g.fillRect (juce::Rectangle<float> (X (ls), r.getY(), X (le) - X (ls), r.getHeight()));
                g.setColour (Colours::accent);
                g.fillRect (X (ls) - 1.0f, r.getY(), 2.0f, r.getHeight());
                g.fillRect (X (le) - 1.0f, r.getY(), 2.0f, r.getHeight());
            }
            g.setColour (Colours::text);
            g.fillRect (X (st) - 1.0f, r.getY(), 2.0f, r.getHeight());
        }
        else   // Grain: позиция и разброс
        {
            const float pos = param ("scan"), spray = param ("grainSpray");
            g.setColour (Colours::accent.withAlpha (0.14f));
            g.fillRect (juce::Rectangle<float> (X (pos - spray * 0.5f), r.getY(), X (pos + spray * 0.5f) - X (pos - spray * 0.5f), r.getHeight()));
            g.setColour (Colours::accent);
            g.fillRect (X (pos) - 1.0f, r.getY(), 2.0f, r.getHeight());
        }
    }
    else
    {
        const Wavetable& t = asset ? asset->table : Wavetable::getBasic();
        const float scanV = param ("scan");
        // призрачные следы прошлых положений Position
        if (ghosts.empty() || std::abs (ghosts.back() - scanV) > 0.004f)
        {
            ghosts.push_back (scanV);
            while (ghosts.size() > 5) ghosts.pop_front();
        }
        const int W = juce::jmax (2, (int) inner.getWidth());
        for (size_t k = 0; k < ghosts.size(); ++k)
        {
            const bool current = k + 1 == ghosts.size();
            juce::Path p;
            for (int x = 0; x < W; ++x)
            {
                const float v = t.read ((double) x / (W - 1) * 0.999, ghosts[k], 0);
                const float y = inner.getCentreY() - v * inner.getHeight() * 0.42f;
                if (x == 0) p.startNewSubPath (inner.getX() + (float) x, y); else p.lineTo (inner.getX() + (float) x, y);
            }
            g.setColour (current ? col : col.withAlpha (0.12f + 0.06f * (float) k));
            g.strokePath (p, juce::PathStrokeType (current ? 2.0f : 1.0f));
        }
    }

    g.setColour (Colours::ink2);
    g.setFont (MistLookAndFeel::font (MistLookAndFeel::Type::footnote));
    const juce::String hint = asset == nullptr ? "built-in table" + midDot() + "drop a sample on the slot"
                            : mode == OpModeSample ? "drag markers: start / loop" : mode == OpModeGrain ? "drag: position" : "";
    g.drawText (hint, r.reduced (8, 4), juce::Justification::bottomRight);
}

void WaveView::mouseDown (const juce::MouseEvent& e)
{
    dragging = nullptr;
    const int mode = (int) param ("mode");
    if (proc.getSample (opIndex) == nullptr) return;
    const float x = juce::jlimit (0.0f, 1.0f, (float) (e.x - 8) / (float) juce::jmax (1, getWidth() - 16));
    if (mode == OpModeGrain || mode == OpModeWave)
        dragging = paramPtr ("scan");
    else
    {
        float best = 0.06f;
        for (auto* n : { "start", "loopStart", "loopEnd" })
        {
            const float d = std::abs (param (n) - x);
            if (d < best) { best = d; dragging = paramPtr (n); }
        }
    }
    if (dragging)
    {
        dragging->beginChangeGesture();
        mouseDrag (e);
    }
}

void WaveView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging == nullptr) return;
    const float x = juce::jlimit (0.0f, 1.0f, (float) (e.x - 8) / (float) juce::jmax (1, getWidth() - 16));
    dragging->setValueNotifyingHost (dragging->convertTo0to1 (x));
    repaint();
}

void WaveView::mouseUp (const juce::MouseEvent&)
{
    if (dragging) dragging->endChangeGesture();
    dragging = nullptr;
}

// ===========================================================================
OperatorPage::OperatorPage (MistProcessor& p) : proc (p), wave (p)
{
    for (auto* k : allKnobs()) addChildComponent (k);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &on, &fixed, &keytrack, &loopMode })
        addChildComponent (c);
    addAndMakeVisible (wave);
    setOperator (0);
}

std::vector<ParamKnob*> OperatorPage::allKnobs()
{
    return { &level, &coarse, &fine, &detune, &feedback, &velSens, &pan, &scan, &start, &loopStart, &loopEnd, &xfade,
             &root, &grainSize, &grainDensity, &grainSpray, &fixedHz, &out };
}

void OperatorPage::setOperator (int newOp)
{
    op = newOp;
    col = Colours::forOperator (op);
    auto& s = proc.apvts;
    level.attach (s, opId (op, "level"));        coarse.attach (s, opId (op, "coarse"));
    fine.attach (s, opId (op, "fine"));          detune.attach (s, opId (op, "detune"));
    feedback.attach (s, opId (op, "feedback"));  velSens.attach (s, opId (op, "velSens"));
    pan.attach (s, opId (op, "pan"));            scan.attach (s, opId (op, "scan"));
    start.attach (s, opId (op, "start"));        loopStart.attach (s, opId (op, "loopStart"));
    loopEnd.attach (s, opId (op, "loopEnd"));    xfade.attach (s, opId (op, "xfade"));
    root.attach (s, opId (op, "root"));          grainSize.attach (s, opId (op, "grainSize"));
    grainDensity.attach (s, opId (op, "grainDensity")); grainSpray.attach (s, opId (op, "grainSpray"));
    fixedHz.attach (s, opId (op, "fixedHz"));    out.attach (s, opId (op, "out"));
    on.attach (s, opId (op, "on"));              fixed.attach (s, opId (op, "fixed"));
    keytrack.attach (s, opId (op, "keytrack"));  loopMode.attach (s, opId (op, "loopMode"));
    for (auto* k : allKnobs()) k->slider.getProperties().set ("accent", "accent");
    for (auto* t : { &on, &fixed, &keytrack }) t->getProperties().set ("accent", "live");
    wave.setOperator (op);
    shownMode = -1;
    refresh();
}

std::vector<juce::Component*> OperatorPage::secondRow()
{
    const int mode = (int) getParam (proc, opId (op, "mode"));
    const bool isFixed = getParam (proc, opId (op, "fixed")) > 0.5f;
    std::vector<juce::Component*> row;
    switch (mode)
    {
        case OpModeSample: row = { &start, &loopStart, &loopEnd, &xfade, &root }; break;
        case OpModeGrain:  row = { &scan, &grainSize, &grainDensity, &grainSpray, &root }; break;
        default:           row = { &scan }; break;
    }
    if (isFixed) row.push_back (&fixedHz);
    if ((int) getParam (proc, "algorithm") == 0) row.push_back (&out);
    return row;
}

void OperatorPage::refresh()
{
    const int mode = (int) getParam (proc, opId (op, "mode"));
    const bool isFixed = getParam (proc, opId (op, "fixed")) > 0.5f;
    const bool matrix = (int) getParam (proc, "algorithm") == 0;
    const int key = mode * 4 + (isFixed ? 2 : 0) + (matrix ? 1 : 0);
    if (key != shownMode)
    {
        shownMode = key;
        resized();
    }
    coarse.setEnabled (! isFixed);
    fine.setEnabled (! isFixed);
    wave.repaint();
}

void OperatorPage::resized()
{
    for (auto* k : allKnobs()) k->setVisible (false);
    auto r = getLocalBounds();
    const int rowH = 92;

    auto row1 = r.removeFromTop (rowH);
    std::vector<juce::Component*> first { &level, &coarse, &fine, &detune, &feedback, &velSens, &pan };
    for (auto* c : first) c->setVisible (true);
    layoutRow (row1, first, 4, 96);

    r.removeFromTop (6);
    auto bottom = r;
    auto toggles = bottom.removeFromRight (120);
    on.setVisible (true); fixed.setVisible (true);
    on.setBounds (toggles.removeFromTop (24));
    fixed.setBounds (toggles.removeFromTop (24));
    const int mode = (int) getParam (proc, opId (op, "mode"));
    keytrack.setVisible (mode != OpModeWave);
    loopMode.setVisible (mode == OpModeSample);
    if (keytrack.isVisible()) keytrack.setBounds (toggles.removeFromTop (24));
    if (loopMode.isVisible()) loopMode.setBounds (toggles.removeFromTop (44).withTrimmedRight (6));

    bottom.removeFromRight (8);
    auto row2 = secondRow();
    for (auto* c : row2) c->setVisible (true);
    const int knobsW = juce::jmin ((int) row2.size() * 96, bottom.getWidth() / 2 + 40);
    auto knobArea = bottom.removeFromRight (knobsW);
    layoutRow (knobArea.withSizeKeepingCentre (knobArea.getWidth(), juce::jmin (rowH, knobArea.getHeight())), row2, 2, 96);
    bottom.removeFromRight (8);
    wave.setBounds (bottom);
}

// ===========================================================================
EnvelopePage::EnvelopePage (MistProcessor& p) : proc (p)
{
    for (auto* k : { &delay, &attack, &hold, &decay, &sustain, &release }) addAndMakeVisible (k);
    addAndMakeVisible (view);
    setOperator (0);
}

void EnvelopePage::setOperator (int newOp)
{
    op = newOp;
    auto& s = proc.apvts;
    delay.attach (s, opId (op, "delay"));     attack.attach (s, opId (op, "attack"));
    hold.attach (s, opId (op, "hold"));       decay.attach (s, opId (op, "decay"));
    sustain.attach (s, opId (op, "sustain")); release.attach (s, opId (op, "release"));
    view.setColour (Colours::forOperator (op));
    refresh();
}

void EnvelopePage::refresh()
{
    auto get = [&] (const char* n) { return getParam (proc, opId (op, n)); };
    view.setValues (get ("delay"), get ("attack"), get ("hold"), get ("decay"), get ("sustain"), get ("release"));
}

void EnvelopePage::resized()
{
    auto r = getLocalBounds();
    layoutRow (r.removeFromTop (92), { &delay, &attack, &hold, &decay, &sustain, &release });
    r.removeFromTop (8);
    view.setBounds (r);
}
} // namespace mist
