#include "PluginEditor.h"

namespace mist
{
// ===========================================================================
void Segmented::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::panelHi.darker (0.03f));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    if (items.empty()) return;
    const float w = (r.getWidth() - 4.0f) / (float) items.size();
    for (size_t i = 0; i < items.size(); ++i)
    {
        auto cell = juce::Rectangle<float> (r.getX() + 2.0f + w * (float) i, r.getY() + 2.0f, w, r.getHeight() - 4.0f);
        const bool sel = items[i].value == selected;
        if (sel)
        {
            juce::Path p;
            p.addRoundedRectangle (cell, cell.getHeight() * 0.5f);
            juce::DropShadow (juce::Colours::black.withAlpha (0.12f), 4, { 0, 1 }).drawForPath (g, p);
            g.setColour (Colours::panel);
            g.fillPath (p);
        }
        g.setColour (sel ? Colours::text : Colours::textDim);
        g.setFont (MistLookAndFeel::font (12.0f, sel));
        g.drawText (items[i].text, cell, juce::Justification::centred);
    }
}

void Segmented::mouseDown (const juce::MouseEvent& e)
{
    if (items.empty()) return;
    const int i = juce::jlimit (0, (int) items.size() - 1, (int) ((float) e.x / (float) getWidth() * (float) items.size()));
    selected = items[(size_t) i].value;
    repaint();
    if (onChange) onChange (selected);
}

// ===========================================================================
PillChoice::PillChoice (APVTS& s, const juce::String& id, const juce::String& cap)
    : param (s.getParameter (id)), caption (cap)
{
    attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v) { index = (int) v; repaint(); }, nullptr);
    attachment->sendInitialUpdate();
}

void PillChoice::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Colours::panelHi);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (Colours::outline);
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
    auto t = getLocalBounds().reduced (12, 0);
    g.setFont (MistLookAndFeel::font (11.5f));
    g.setColour (Colours::textDim);
    const int cw = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), caption);
    g.drawText (caption, t.removeFromLeft (cw + 6), juce::Justification::centredLeft);
    g.setColour (Colours::text);
    g.setFont (MistLookAndFeel::font (11.5f, true));
    g.drawText (param->getCurrentValueAsText(), t.withTrimmedRight (14), juce::Justification::centredLeft);
    juce::Path arrow;
    const float ax = (float) getWidth() - 16.0f, ay = (float) getHeight() * 0.5f;
    arrow.startNewSubPath (ax - 3, ay - 1.5f); arrow.lineTo (ax, ay + 1.5f); arrow.lineTo (ax + 3, ay - 1.5f);
    g.strokePath (arrow, juce::PathStrokeType (1.3f));
}

void PillChoice::mouseDown (const juce::MouseEvent&)
{
    auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param);
    if (choice == nullptr) return;
    juce::PopupMenu m;
    for (int i = 0; i < choice->choices.size(); ++i)
        m.addItem (i + 1, choice->choices[i], true, i == index);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this] (int r)
    {
        if (r > 0) attachment->setValueAsCompleteGesture ((float) (r - 1));
    });
}

// ===========================================================================
void OutMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    g.setFont (MistLookAndFeel::mono (10.5f));
    g.setColour (Colours::textDim);
    g.drawText ("Out", r.removeFromLeft (28), juce::Justification::centredLeft);
    const float db = juce::Decibels::gainToDecibels (level, -60.0f);
    auto txt = r.removeFromRight (46);
    g.drawText (level > 0.001f ? juce::String (db, 1) + " dB" : juce::String ("-inf"), txt, juce::Justification::centredRight);
    auto bar = r.reduced (2, 0).toFloat().withSizeKeepingCentre ((float) r.getWidth() - 4.0f, 5.0f);
    g.setColour (Colours::track);
    g.fillRoundedRectangle (bar, 2.5f);
    const float t = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    g.setColour (db > -1.0f ? Colours::danger : db > -6.0f ? juce::Colour (0xffffb020) : Colours::good);
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * t), 2.5f);
}

// ===========================================================================
MistEditor::MistEditor (MistProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      opPage (p), envPage (p), routing (p), voicePage (p), filterPage (p), lfo1 (p, 0), lfo2 (p, 1), modPage (p),
      interp (p.apvts, "interp", "Interpolation"), oversampling (p.apvts, "oversampling", "Oversampling")
{
    applyTheme (loadSavedTheme());
    lnf.refreshColours();
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    // шапка
    for (juce::Component* c : std::initializer_list<juce::Component*> { &prevPreset, &nextPreset, &presetName, &settingsButton, &modeSwitch, &outMeter })
        content.addAndMakeVisible (c);
    for (auto* b : { &prevPreset, &nextPreset, &presetName, &settingsButton })
        b->getProperties().set ("noOutline", true);
    prevPreset.onClick = [this] { proc.presets.loadNext (-1); };
    nextPreset.onClick = [this] { proc.presets.loadNext (1); };
    presetName.onClick = [this] { showPresetMenu(); };
    settingsButton.onClick = [this] { showSettingsMenu(); };
    modeSwitch.setItems ({ { "Sample PM", OpModeSample }, { "Scrub", OpModeWave }, { "Grain", OpModeGrain } });
    modeSwitch.onChange = [this] (int v) { setParam (opId (selectedOp, "mode"), (float) v); };

    content.addAndMakeVisible (viz);
    viz.onDroneClicked = [this]
    {
        auto* d = proc.apvts.getParameter ("drone");
        d->beginChangeGesture();
        d->setValueNotifyingHost (d->getValue() > 0.5f ? 0.0f : 1.0f);
        d->endChangeGesture();
    };

    for (int i = 0; i < kNumOps; ++i)
    {
        slots.push_back (std::make_unique<SampleSlot> (proc, i, [this] (int op) { selectOperator (op); }));
        content.addAndMakeVisible (*slots.back());
    }

    // вкладки
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

    // низ
    drone.attach (proc.apvts, "drone");
    drone.getProperties().set ("accent", "good");
    cpuLabel.setFont (MistLookAndFeel::mono (10.5f));
    styleLabel (cpuLabel, true);
    cpuLabel.setJustificationType (juce::Justification::centredRight);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &drone, &interp, &oversampling, &cpuLabel })
        content.addAndMakeVisible (c);

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
// Тема — глобальная настройка пользователя (~/Library/Application Support/Mist/Mist.settings)
static std::unique_ptr<juce::PropertiesFile> openPrefs()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "Mist";
    o.filenameSuffix = "settings";
    o.folderName = "Mist";
    o.osxLibrarySubFolder = "Application Support";
    return std::make_unique<juce::PropertiesFile> (o);
}

ThemeId MistEditor::loadSavedTheme()
{
    return openPrefs()->getIntValue ("theme", 0) == 1 ? ThemeId::Dark : ThemeId::Light;
}

void MistEditor::setTheme (ThemeId id)
{
    applyTheme (id);
    lnf.refreshColours();
    if (auto p = openPrefs()) { p->setValue ("theme", (int) id); p->saveIfNeeded(); }
    restyleTree (*this);
    sendLookAndFeelChange();
    repaint();
}

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
    juce::PopupMenu size;
    for (int pct : { 75, 100, 125, 150 })
        size.addItem (100 + pct, juce::String (pct) + "%", true, std::abs (getWidth() - kBaseW * pct / 100) < 4);
    m.addSubMenu ("Window size", size);
    juce::PopupMenu theme;
    theme.addItem (201, "Light", true, currentTheme() == ThemeId::Light);
    theme.addItem (202, "Dark", true, currentTheme() == ThemeId::Dark);
    m.addSubMenu ("Theme", theme);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (settingsButton), [this] (int r)
    {
        if (r == 1) savePreset();
        else if (r == 2) proc.presets.initPatch();
        else if (r == 3) { auto f = PresetManager::getPresetFolder(); f.createDirectory(); f.startAsProcess(); }
        else if (r == 4) proc.presets.deleteUserPreset (proc.presets.getCurrentIndex());
        else if (r == 201) setTheme (ThemeId::Light);
        else if (r == 202) setTheme (ThemeId::Dark);
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
    g.fillAll (Colours::bg);
    const float s = (float) getWidth() / (float) kBaseW;
    g.addTransform (juce::AffineTransform::scale (s));

    // шапка
    drawCard (g, headerArea.toFloat(), 12.0f, true);
    g.setColour (Colours::text);
    g.setFont (MistLookAndFeel::pixel (26.0f));
    g.drawText ("Mist", headerArea.withTrimmedLeft (18).withWidth (60), juce::Justification::centredLeft);
    g.setColour (Colours::outline);
    g.drawVerticalLine (headerArea.getX() + 74, (float) headerArea.getY() + 14, (float) headerArea.getBottom() - 14);

    // карточка настроек
    drawCard (g, settingsArea.toFloat(), 14.0f, true);
    g.setColour (Colours::text);
    g.setFont (MistLookAndFeel::font (15.0f, true));
    auto ta = titleArea;
    g.drawText (pageTitle, ta.removeFromTop (20), juce::Justification::centredLeft);
    g.setColour (Colours::textDim);
    g.setFont (MistLookAndFeel::font (11.5f));
    g.drawText (pageDescription, titleArea.withHeight (18).translated (0, 22), juce::Justification::centredLeft);

    // нижняя панель
    g.setColour (Colours::outline);
    g.drawHorizontalLine (bottomArea.getY(), (float) bottomArea.getX() + 4, (float) bottomArea.getRight() - 4);
}

void MistEditor::paintOverChildren (juce::Graphics& g)
{
    // иконка настроек (два «фейдера»), как в макете
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / (float) kBaseW));
    auto r = settingsButton.getBounds().toFloat().withSizeKeepingCentre (14.0f, 10.0f);
    g.setColour (Colours::text);
    for (int i = 0; i < 2; ++i)
    {
        const float y = r.getY() + (float) i * r.getHeight();
        g.drawLine (r.getX(), y, r.getRight(), y, 1.3f);
        const float kx = i == 0 ? r.getX() + 4.0f : r.getRight() - 4.0f;
        g.setColour (Colours::panelHi);
        g.fillEllipse (kx - 2.5f, y - 2.5f, 5.0f, 5.0f);
        g.setColour (Colours::text);
        g.drawEllipse (kx - 2.5f, y - 2.5f, 5.0f, 5.0f, 1.2f);
    }
}

void MistEditor::resized()
{
    const float scale = (float) getWidth() / (float) kBaseW;
    content.setBounds (0, 0, kBaseW, kBaseH);
    content.setTransform (juce::AffineTransform::scale (scale));

    auto r = juce::Rectangle<int> (0, 0, kBaseW, kBaseH).reduced (14);

    // шапка
    headerArea = r.removeFromTop (48);
    {
        auto h = headerArea.reduced (10, 9);
        h.removeFromLeft (72);
        prevPreset.setBounds (h.removeFromLeft (28));
        presetName.setBounds (h.removeFromLeft (190));
        nextPreset.setBounds (h.removeFromLeft (28));
        settingsButton.setBounds (h.removeFromRight (34));
        h.removeFromRight (10);
        outMeter.setBounds (h.removeFromRight (150));
        modeSwitch.setBounds (headerArea.withSizeKeepingCentre (250, 30));
    }
    r.removeFromTop (10);

    viz.setBounds (r.removeFromTop (262));
    r.removeFromTop (10);

    // левая колонка — слоты семплов
    auto left = r.removeFromLeft (318);
    const int gap = 8;
    const int slotH = (left.getHeight() - gap * (kNumOps - 1)) / kNumOps;
    for (auto& s : slots)
    {
        s->setBounds (left.removeFromTop (slotH));
        left.removeFromTop (gap);
    }
    r.removeFromLeft (10);

    // карточка настроек
    settingsArea = r;
    auto inner = r.reduced (18, 14);
    bottomArea = inner.removeFromBottom (40);
    tabs.setBounds (inner.removeFromTop (30).withWidth (juce::jmin (inner.getWidth(), 620)));
    inner.removeFromTop (12);
    titleArea = inner.removeFromTop (42);
    inner.removeFromTop (10);
    for (auto* pg : pages) pg->setBounds (inner);
    {
        auto l = lfoPage.getLocalBounds();
        lfo1.setBounds (l.removeFromLeft (l.getWidth() / 2 - 5));
        l.removeFromLeft (10);
        lfo2.setBounds (l);
    }

    auto b = bottomArea.withTrimmedTop (8);
    drone.setBounds (b.removeFromLeft (84).withSizeKeepingCentre (84, 24));
    b.removeFromLeft (8);
    interp.setBounds (b.removeFromLeft (170).withSizeKeepingCentre (170, 26));
    b.removeFromLeft (8);
    oversampling.setBounds (b.removeFromLeft (150).withSizeKeepingCentre (150, 26));
    cpuLabel.setBounds (b);
}

void MistEditor::timerCallback()
{
    // аудио → визуализация
    const int ready = proc.scopeFifo.getNumReady();
    if (ready > 0)
    {
        const auto rd = proc.scopeFifo.read (ready);
        if (rd.blockSize1 > 0) viz.push (proc.scopeL.data() + rd.startIndex1, proc.scopeR.data() + rd.startIndex1, rd.blockSize1);
        if (rd.blockSize2 > 0) viz.push (proc.scopeL.data() + rd.startIndex2, proc.scopeR.data() + rd.startIndex2, rd.blockSize2);
    }
    viz.setSampleRate (proc.getSampleRate());
    auto* d = proc.apvts.getParameter ("drone");
    viz.setDroneState (d->getValue() > 0.5f, proc.lowestNote.load());
    viz.tick();

    // шапка
    presetName.setButtonText (proc.presets.getCurrentName());
    outMeter.setLevel (proc.outPeak.load());
    auto* m = proc.apvts.getParameter (opId (selectedOp, "mode"));
    modeSwitch.setSelectedValue ((int) m->convertFrom0to1 (m->getValue()));

    cpuLabel.setText ("Voices " + juce::String (proc.activeVoices.load()) + "/" + juce::String (kNumVoices)
                      + "      CPU " + juce::String (juce::roundToInt (proc.cpuLoad.load() * 100.0f)) + "%",
                      juce::dontSendNotification);

    for (auto& s : slots) s->refresh();
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
