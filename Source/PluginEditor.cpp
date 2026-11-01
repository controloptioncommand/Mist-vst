#include "PluginEditor.h"

namespace mist
{
using LnF = MistLookAndFeel;

// ===========================================================================
Segmented::Segmented()
{
    setWantsKeyboardFocus (true);
}

void Segmented::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float rad = r.getHeight() * 0.5f;
    g.setColour (Colours::well);
    g.fillRoundedRectangle (r, rad);
    g.setColour (Colours::outline);
    g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
    if (items.empty()) return;
    const float inset = 2.0f;
    const float w = (r.getWidth() - inset * 2) / (float) items.size();
    for (size_t i = 0; i < items.size(); ++i)
    {
        // концентрично: радиус сегмента = радиус дорожки − отступ
        auto cell = juce::Rectangle<float> (r.getX() + inset + w * (float) i, r.getY() + inset, w, r.getHeight() - inset * 2);
        const bool sel = items[i].value == selected;
        if (sel)
        {
            g.setColour (Colours::surface);
            g.fillRoundedRectangle (cell, rad - inset);
            g.setColour (Colours::outline);
            g.drawRoundedRectangle (cell.reduced (0.5f), rad - inset, 1.0f);
        }
        g.setColour (sel ? Colours::ink : Colours::ink2);
        g.setFont (LnF::font (LnF::Type::callout, sel));
        g.drawText (items[i].text, cell, juce::Justification::centred);
    }
    if (hasKeyboardFocus (false)) LnF::drawFocusRing (g, r, rad);
}

void Segmented::choose (int i)
{
    if (items.empty()) return;
    i = juce::jlimit (0, (int) items.size() - 1, i);
    selected = items[(size_t) i].value;
    repaint();
    if (onChange) onChange (selected);
}

void Segmented::mouseDown (const juce::MouseEvent& e)
{
    choose ((int) ((float) e.x / (float) getWidth() * (float) items.size()));
}

bool Segmented::keyPressed (const juce::KeyPress& k)
{
    int cur = 0;
    for (size_t i = 0; i < items.size(); ++i) if (items[i].value == selected) cur = (int) i;
    if (k.getKeyCode() == juce::KeyPress::leftKey)  { choose (cur - 1); return true; }
    if (k.getKeyCode() == juce::KeyPress::rightKey) { choose (cur + 1); return true; }
    return false;
}

// ===========================================================================
PillChoice::PillChoice (APVTS& s, const juce::String& id, const juce::String& cap)
    : param (s.getParameter (id)), caption (cap)
{
    setWantsKeyboardFocus (true);
    setTitle (cap);
    attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v)
    {
        index = (int) v;
        setDescription (caption + " " + param->getCurrentValueAsText());
        repaint();
    }, nullptr);
    attachment->sendInitialUpdate();
}

void PillChoice::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const float rad = r.getHeight() * 0.5f;
    g.setColour (Colours::well);
    g.fillRoundedRectangle (r, rad);
    g.setColour (Colours::outline);
    g.drawRoundedRectangle (r, rad, 1.0f);
    auto t = getLocalBounds().reduced (12, 0);
    g.setFont (LnF::font (LnF::Type::callout));
    g.setColour (Colours::ink2);
    const int cw = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), caption);
    g.drawText (caption, t.removeFromLeft (cw + 6), juce::Justification::centredLeft);
    g.setColour (Colours::ink);
    auto valueText = param->getCurrentValueAsText().replace ("x", juce::String::fromUTF8 ("\xc3\x97"));
    g.setFont (LnF::font (LnF::Type::callout, true));
    g.drawText (valueText, t.withTrimmedRight (14), juce::Justification::centredLeft);
    juce::Path arrow;
    const float ax = (float) getWidth() - 16.0f, ay = (float) getHeight() * 0.5f;
    arrow.startNewSubPath (ax - 3, ay - 1.5f); arrow.lineTo (ax, ay + 1.5f); arrow.lineTo (ax + 3, ay - 1.5f);
    g.strokePath (arrow, juce::PathStrokeType (1.3f));
    if (hasKeyboardFocus (false)) LnF::drawFocusRing (g, r, rad);
}

void PillChoice::openMenu()
{
    auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param);
    if (choice == nullptr) return;
    juce::PopupMenu m;
    for (int i = 0; i < choice->choices.size(); ++i)
        m.addItem (i + 1, choice->choices[i].replace ("x", juce::String::fromUTF8 ("\xc3\x97")), true, i == index);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this] (int r)
    {
        if (r > 0) attachment->setValueAsCompleteGesture ((float) (r - 1));
    });
}

bool PillChoice::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::returnKey || k == juce::KeyPress::spaceKey) { openMenu(); return true; }
    return false;
}

// ===========================================================================
void OutMeter::setLevel (float peak)
{
    level = peak;
    const float db = juce::Decibels::gainToDecibels (level, -60.0f);
    setDescription ("Output peak " + (level > 0.001f ? juce::String (db, 1) + " dB" : juce::String ("silent")));
    repaint();
}

void OutMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    g.setFont (LnF::font (LnF::Type::subheadline));
    g.setColour (Colours::ink2);
    g.drawText ("Out", r.removeFromLeft (28), juce::Justification::centredLeft);
    const float db = juce::Decibels::gainToDecibels (level, -60.0f);
    auto txt = r.removeFromRight (52);
    g.setFont (LnF::mono (LnF::Type::footnote));
    g.setColour (Colours::ink);
    g.drawText (level > 0.001f ? juce::String (juce::roundToInt (db)).replace ("-", juce::String::fromUTF8 ("\xe2\x88\x92")) + " dB"
                               : juce::String::fromUTF8 ("\xe2\x88\x92\xe2\x88\x9e"), txt, juce::Justification::centredRight);
    auto bar = r.reduced (2, 0).toFloat().withSizeKeepingCentre ((float) r.getWidth() - 4.0f, 5.0f);
    g.setColour (Colours::track);
    g.fillRoundedRectangle (bar, 2.5f);
    const float t = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    g.setColour (db > -1.0f ? Colours::signal : Colours::live);
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * t), 2.5f);
}

// ===========================================================================
void LogoButton::paintButton (juce::Graphics& g, bool over, bool)
{
    g.setColour (over ? Colours::ink.withAlpha (0.75f) : Colours::ink);
    g.setFont (LnF::pixel (24.0f));
    g.drawText ("Mist", getLocalBounds(), juce::Justification::centred);
    if (hasKeyboardFocus (false)) LnF::drawFocusRing (g, getLocalBounds().toFloat(), 8.0f);
}

void IconButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat();
    if (over || down)
    {
        g.setColour (Colours::ink.withAlpha (down ? 0.10f : 0.05f));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    }
    g.setColour (Colours::ink);
    const auto c = r.getCentre();
    if (kind == Settings)
    {
        auto ir = juce::Rectangle<float> (16.0f, 11.0f).withCentre (c);
        for (int i = 0; i < 2; ++i)
        {
            const float y = ir.getY() + (float) i * ir.getHeight();
            g.drawLine (ir.getX(), y, ir.getRight(), y, 1.4f);
            const float kx = i == 0 ? ir.getX() + 5.0f : ir.getRight() - 5.0f;
            g.setColour (Colours::surface);
            g.fillEllipse (kx - 2.6f, y - 2.6f, 5.2f, 5.2f);
            g.setColour (Colours::ink);
            g.drawEllipse (kx - 2.6f, y - 2.6f, 5.2f, 5.2f, 1.3f);
        }
    }
    else
    {
        const float d = kind == Prev ? 1.0f : -1.0f;
        juce::Path p;
        p.startNewSubPath (c.x + 2.5f * d, c.y - 5.0f);
        p.lineTo (c.x - 2.5f * d, c.y);
        p.lineTo (c.x + 2.5f * d, c.y + 5.0f);
        g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    if (hasKeyboardFocus (false)) LnF::drawFocusRing (g, r, r.getHeight() * 0.5f);
}

// ===========================================================================
static std::unique_ptr<juce::PropertiesFile> openPrefs()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "Mist";
    o.filenameSuffix = "settings";
    o.folderName = "Mist";
    o.osxLibrarySubFolder = "Application Support";
    return std::make_unique<juce::PropertiesFile> (o);
}

MistEditor::MistEditor (MistProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      opPage (p), envPage (p), routing (p), voicePage (p), filterPage (p), lfo1 (p, 0), lfo2 (p, 1), modPage (p),
      interp (p.apvts, "interp", "Interpolation"), oversampling (p.apvts, "oversampling", "Oversampling")
{
    Accessibility::refresh();
    themeMode = (ThemeMode) juce::jlimit (0, 2, openPrefs()->getIntValue ("themeMode", 0));
    applyThemeMode();
    setLookAndFeel (&lnf);
    juce::Desktop::getInstance().addDarkModeSettingListener (this);
    addAndMakeVisible (content);

    // тулбар
    for (juce::Component* c : std::initializer_list<juce::Component*> { &logo, &prevPreset, &presetName, &nextPreset, &modeSwitch, &outMeter, &settingsButton })
        content.addAndMakeVisible (c);
    presetName.getProperties().set ("noOutline", true);
    presetName.setTitle ("Preset");
    logo.onClick = [this] { showAbout (true); };
    prevPreset.onClick = [this] { proc.presets.loadNext (-1); };
    nextPreset.onClick = [this] { proc.presets.loadNext (1); };
    presetName.onClick = [this] { showPresetMenu(); };
    settingsButton.onClick = [this] { showSettingsMenu(); };
    modeSwitch.setTitle ("Operator mode");
    modeSwitch.setItems ({ { "Sample PM", OpModeSample }, { "Scrub", OpModeWave }, { "Grain", OpModeGrain } });
    modeSwitch.onChange = [this] (int v) { setParam (opId (selectedOp, "mode"), (float) v); };

    content.addAndMakeVisible (viz);

    for (int i = 0; i < kNumOps; ++i)
    {
        slots.push_back (std::make_unique<SampleSlot> (proc, i, [this] (int op) { selectOperator (op); }));
        content.addAndMakeVisible (*slots.back());
    }

    tabs.setTitle ("Operator panel tabs");
    tabs.setItems ({ { "Operator", 0 }, { "Envelope", 1 }, { "Routing", 2 }, { "Voice", 3 }, { "Filter", 4 }, { "LFO", 5 }, { "Mod Matrix", 6 } });
    tabs.onChange = [this] (int v) { showPage (v); };
    content.addAndMakeVisible (tabs);

    routing.setFlat (true);
    filterPage.setFlat (true);
    modPage.setFlat (true);
    lfoPage.addAndMakeVisible (lfo1);
    lfoPage.addAndMakeVisible (lfo2);
    pages = { &opPage, &envPage, &routing, &voicePage, &filterPage, &lfoPage, &modPage };
    for (auto* pg : pages) content.addChildComponent (pg);
    routing.diagram.onOperatorClicked = [this] (int op) { selectOperator (op); };

    drone.attach (proc.apvts, "drone");
    drone.getProperties().set ("accent", "live");
    statusLabel.setFont (LnF::mono (LnF::Type::footnote));
    styleLabel (statusLabel, true);
    statusLabel.setJustificationType (juce::Justification::centredRight);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &drone, &interp, &oversampling, &statusLabel })
        content.addAndMakeVisible (c);

    content.addChildComponent (about);
    about.onClose = [this] { showAbout (false); };

    updateRoles();
    selectOperator (0);
    showPage (0);

    setResizable (true, true);
    setResizeLimits (kBaseW / 2, kBaseH / 2, kBaseW * 2, kBaseH * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) kBaseW / kBaseH);
    setSize (kBaseW, kBaseH);
    viz.setSampleRate (proc.getSampleRate());
    startTimerHz (30);
}

MistEditor::~MistEditor()
{
    stopTimer();
    juce::Desktop::getInstance().removeDarkModeSettingListener (this);
    setLookAndFeel (nullptr);
}

void MistEditor::setParam (const juce::String& id, float realValue)
{
    if (auto* p = proc.apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
        p->endChangeGesture();
    }
}

// ---------------------------------------------------------------------------
// Тема: по системе (светлая/тёмная macOS) или вручную
void MistEditor::applyThemeMode()
{
    ThemeId id = ThemeId::Oxygen;
    if (themeMode == ThemeMode::Rubber) id = ThemeId::Rubber;
    else if (themeMode == ThemeMode::System) id = juce::Desktop::getInstance().isDarkModeActive() ? ThemeId::Rubber : ThemeId::Oxygen;
    applyTheme (id);
    lnf.refreshColours();
    restyleTree (*this);
    sendLookAndFeelChange();
    repaint();
}

void MistEditor::setThemeMode (ThemeMode m)
{
    themeMode = m;
    if (auto p = openPrefs()) { p->setValue ("themeMode", (int) m); p->saveIfNeeded(); }
    applyThemeMode();
}

void MistEditor::darkModeSettingChanged()
{
    if (themeMode == ThemeMode::System) applyThemeMode();
}

// ---------------------------------------------------------------------------
void MistEditor::updateRoles()
{
    const auto r = Routing::read (proc);
    for (int i = 0; i < kNumOps; ++i)
    {
        int role = 2;
        if (r.on[i])
        {
            if (r.out[i] > 0.0f) role = 0;
            else for (int d = 0; d < kNumOps; ++d) if (r.mod[i][d] > 0.0f) { role = 1; break; }
        }
        Colours::opRole[(size_t) i] = role;
    }
}

void MistEditor::selectOperator (int op)
{
    selectedOp = op;
    for (int i = 0; i < kNumOps; ++i) slots[(size_t) i]->setSelected (i == op);
    opPage.setOperator (op);
    envPage.setOperator (op);
    updatePageTitle();
}

void MistEditor::showPage (int pg)
{
    page = pg;
    tabs.setSelectedValue (pg);
    for (size_t i = 0; i < pages.size(); ++i) pages[i]->setVisible ((int) i == pg);
    updatePageTitle();
}

void MistEditor::showAbout (bool show)
{
    if (! show) about.showGame (false);
    about.setVisible (show);
    if (show) about.toFront (true);
}

void MistEditor::updatePageTitle()
{
    auto* m = proc.apvts.getParameter (opId (selectedOp, "mode"));
    const int mode = (int) m->convertFrom0to1 (m->getValue());
    const juce::String opName = "Op " + juce::String (selectedOp + 1) + midDot();
    juce::String t, d;
    switch (page)
    {
        case 0: t = opName + modeName (mode); d = modeDescription (mode); break;
        case 1: t = opName + "Envelope"; d = "Delay, attack, hold, decay, sustain, release. Times up to 30 s for slow drones."; break;
        case 2: t = "Routing"; d = "32 algorithms or a free 6x6 matrix. Loops and back-links run with a one-sample delay."; break;
        case 3: t = "Voice"; d = "Polyphony, glide, unison and the global FM depth."; break;
        case 4: t = "Filter"; d = "Per-voice filter with drive. The Mod Env also feeds the mod matrix."; break;
        case 5: t = "LFO"; d = "Two global LFOs, free or synced to the host tempo."; break;
        default: t = "Mod Matrix"; d = "Eight slots: any source to any destination, bipolar amount."; break;
    }
    if (t != pageTitle || d != pageDescription)
    {
        pageTitle = t;
        pageDescription = d;
        repaint();
    }
}

// ---------------------------------------------------------------------------
void MistEditor::showPresetMenu()
{
    proc.presets.refresh();
    juce::PopupMenu menu;
    std::map<juce::String, juce::PopupMenu> cats;
    juce::StringArray order;
    const auto& e = proc.presets.getEntries();
    for (int i = 0; i < e.size(); ++i)
    {
        if (! order.contains (e[i].category)) order.add (e[i].category);
        cats[e[i].category].addItem (i + 1, e[i].name, true, i == proc.presets.getCurrentIndex());
    }
    for (auto& c : order) menu.addSubMenu (c, cats[c]);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetName), [this] (int r)
    {
        if (r > 0) proc.presets.load (r - 1);
    });
}

void MistEditor::showSettingsMenu()
{
    juce::PopupMenu m;
    m.addItem (1, "Save preset...");
    m.addItem (2, "Init patch");
    m.addItem (3, "Open presets folder");
    const int cur = proc.presets.getCurrentIndex();
    m.addItem (4, "Delete current preset", cur >= 0 && proc.presets.getEntries()[cur].factoryIndex < 0);
    m.addSeparator();
    juce::PopupMenu appearance;
    appearance.addItem (201, "Match system", true, themeMode == ThemeMode::System);
    appearance.addSeparator();
    appearance.addItem (202, "Oxygen (light)", true, themeMode == ThemeMode::Oxygen);
    appearance.addItem (203, "Rubber (dark)", true, themeMode == ThemeMode::Rubber);
    m.addSubMenu ("Appearance", appearance);
    juce::PopupMenu size;
    for (int pct : { 75, 100, 125, 150 })
        size.addItem (100 + pct, juce::String (pct) + "%", true, std::abs (getWidth() - kBaseW * pct / 100) < 4);
    m.addSubMenu ("Window size", size);
    m.addSeparator();
    m.addItem (5, "About Mist");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (settingsButton), [this] (int r)
    {
        if (r == 1) savePreset();
        else if (r == 2) proc.presets.initPatch();
        else if (r == 3) { auto f = PresetManager::getPresetFolder(); f.createDirectory(); f.startAsProcess(); }
        else if (r == 4) proc.presets.deleteUserPreset (proc.presets.getCurrentIndex());
        else if (r == 5) showAbout (true);
        else if (r == 201) setThemeMode (ThemeMode::System);
        else if (r == 202) setThemeMode (ThemeMode::Oxygen);
        else if (r == 203) setThemeMode (ThemeMode::Rubber);
        else if (r > 100 && r < 200) setSize (kBaseW * (r - 100) / 100, kBaseH * (r - 100) / 100);
    });
}

void MistEditor::savePreset()
{
    saveWindow = std::make_unique<juce::AlertWindow> ("Save preset", "Samples are stored inside the preset file.",
                                                      juce::MessageBoxIconType::NoIcon, this);
    saveWindow->addTextEditor ("name", proc.presets.getCurrentName(), "Name");
    saveWindow->addTextEditor ("category", "User", "Folder");
    saveWindow->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveWindow->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    saveWindow->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int r)
    {
        if (r == 1 && saveWindow != nullptr)
            if (! proc.presets.save (saveWindow->getTextEditorContents ("name"), saveWindow->getTextEditorContents ("category")))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Mist", "Could not save preset.");
        saveWindow.reset();
    }), false);
}

// ---------------------------------------------------------------------------
void MistEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::window);
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / (float) kBaseW));

    // тулбар — стекло
    LnF::drawGlass (g, toolbarArea.toFloat(), 18.0f);
    g.setColour (Colours::outline);
    g.drawVerticalLine (logo.getRight() + 8, (float) toolbarArea.getY() + 15, (float) toolbarArea.getBottom() - 15);

    // панель оператора — плотная поверхность
    const float radius = 14.0f;
    g.setColour (Colours::surface);
    g.fillRoundedRectangle (settingsArea.toFloat(), radius);
    g.setColour (Colours::outline);
    g.drawRoundedRectangle (settingsArea.toFloat().reduced (0.5f), radius, 1.0f);

    g.setColour (Colours::ink);
    g.setFont (LnF::font (LnF::Type::title3, true));
    auto ta = titleArea;
    g.drawText (pageTitle, ta.removeFromTop (20), juce::Justification::centredLeft);
    g.setColour (Colours::ink2);
    g.setFont (LnF::font (LnF::Type::callout));
    g.drawText (pageDescription, ta.removeFromTop (18), juce::Justification::centredLeft);

    g.setColour (Colours::outline);
    g.drawHorizontalLine (bottomArea.getY(), (float) bottomArea.getX(), (float) bottomArea.getRight());
}

void MistEditor::layoutSlots()
{
    // заполненные карточки выше пустых
    int mask = 0;
    for (int i = 0; i < kNumOps; ++i) if (slots[(size_t) i]->hasSample()) mask |= 1 << i;
    filledMask = mask;
    const int gap = 8;
    float units = 0;
    for (int i = 0; i < kNumOps; ++i) units += (mask >> i & 1) ? 2.6f : 1.0f;
    const float unitH = (float) (slotsArea.getHeight() - gap * (kNumOps - 1)) / units;
    float y = (float) slotsArea.getY();
    for (int i = 0; i < kNumOps; ++i)
    {
        const float h = unitH * ((mask >> i & 1) ? 2.6f : 1.0f);
        slots[(size_t) i]->setBounds (slotsArea.getX(), juce::roundToInt (y), slotsArea.getWidth(), juce::roundToInt (h));
        y += h + (float) gap;
    }
}

void MistEditor::resized()
{
    content.setBounds (0, 0, kBaseW, kBaseH);
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) kBaseW));
    about.setBounds (0, 0, kBaseW, kBaseH);

    auto r = juce::Rectangle<int> (0, 0, kBaseW, kBaseH).reduced (14);

    // тулбар 52 px; элементы — 32 px (цель нажатия ≥ 28)
    toolbarArea = r.removeFromTop (52);
    {
        auto h = toolbarArea.reduced (10, 10);
        logo.setBounds (h.removeFromLeft (64));
        h.removeFromLeft (18);
        prevPreset.setBounds (h.removeFromLeft (32));
        presetName.setBounds (h.removeFromLeft (170));
        nextPreset.setBounds (h.removeFromLeft (32));
        settingsButton.setBounds (h.removeFromRight (32));
        h.removeFromRight (10);
        outMeter.setBounds (h.removeFromRight (160));
        modeSwitch.setBounds (toolbarArea.withSizeKeepingCentre (270, 32));
    }
    r.removeFromTop (10);

    viz.setBounds (r.removeFromTop (262));
    r.removeFromTop (10);

    slotsArea = r.removeFromLeft (300);
    layoutSlots();
    r.removeFromLeft (10);

    settingsArea = r;
    auto inner = r.reduced (18, 14);
    bottomArea = inner.removeFromBottom (44);
    tabs.setBounds (inner.removeFromTop (30).withWidth (juce::jmin (inner.getWidth(), 600)));
    inner.removeFromTop (12);
    titleArea = inner.removeFromTop (40);
    inner.removeFromTop (8);
    for (auto* pg : pages) pg->setBounds (inner);
    {
        auto l = lfoPage.getLocalBounds();
        lfo1.setBounds (l.removeFromLeft (l.getWidth() / 2 - 5));
        l.removeFromLeft (10);
        lfo2.setBounds (l);
    }

    auto b = bottomArea.withTrimmedTop (10);
    drone.setBounds (b.removeFromLeft (92).withSizeKeepingCentre (92, 28));
    b.removeFromLeft (8);
    interp.setBounds (b.removeFromLeft (172).withSizeKeepingCentre (172, 28));
    b.removeFromLeft (8);
    oversampling.setBounds (b.removeFromLeft (156).withSizeKeepingCentre (156, 28));
    statusLabel.setBounds (b);
}

void MistEditor::timerCallback()
{
    const int ready = proc.scopeFifo.getNumReady();
    if (ready > 0)
    {
        const auto rd = proc.scopeFifo.read (ready);
        if (! about.isVisible())
        {
            if (rd.blockSize1 > 0) viz.push (proc.scopeL.data() + rd.startIndex1, proc.scopeR.data() + rd.startIndex1, rd.blockSize1);
            if (rd.blockSize2 > 0) viz.push (proc.scopeL.data() + rd.startIndex2, proc.scopeR.data() + rd.startIndex2, rd.blockSize2);
        }
    }
    if (about.isVisible()) return;

    viz.setSampleRate (proc.getSampleRate());
    viz.tick();

    presetName.setButtonText (proc.presets.getCurrentName());
    presetName.setDescription ("Preset " + proc.presets.getCurrentName());
    outMeter.setLevel (proc.outPeak.load());
    auto* m = proc.apvts.getParameter (opId (selectedOp, "mode"));
    modeSwitch.setSelectedValue ((int) m->convertFrom0to1 (m->getValue()));

    statusLabel.setText ("Voices " + juce::String (proc.activeVoices.load()) + "/" + juce::String (kNumVoices)
                         + "   CPU " + juce::String (juce::roundToInt (proc.cpuLoad.load() * 100.0f)) + "%",
                         juce::dontSendNotification);

    updateRoles();
    for (auto& s : slots) s->refresh();
    int mask = 0;
    for (int i = 0; i < kNumOps; ++i) if (slots[(size_t) i]->hasSample()) mask |= 1 << i;
    if (mask != filledMask) layoutSlots();

    updatePageTitle();
    switch (page)
    {
        case 0: opPage.refresh(); break;
        case 1: envPage.refresh(); break;
        case 2: routing.refresh(); break;
        case 5: lfo1.refresh(); lfo2.refresh(); break;
        default: break;
    }
}
} // namespace mist
