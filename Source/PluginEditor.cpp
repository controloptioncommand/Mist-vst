#include "PluginEditor.h"

namespace ferrite
{
// ===========================================================================
HeaderBar::HeaderBar (FerriteProcessor& p) : proc (p)
{
    auto& s = proc.apvts;
    polyMode.attach (s, "polyMode");
    glide.attach (s, "glide"); bend.attach (s, "bendRange"); uni.attach (s, "uniVoices");
    uniDetune.attach (s, "uniDetune"); uniSpread.attach (s, "uniSpread");
    fm.attach (s, "fmAmount"); master.attach (s, "master");

    for (juce::Component* c : std::initializer_list<juce::Component*> { &presetButton, &prev, &next, &save, &init, &polyMode, &glide, &bend,
                                                                        &uni, &uniDetune, &uniSpread, &fm, &master, &scope, &voicesLabel })
        addAndMakeVisible (c);

    voicesLabel.setFont (FerriteLookAndFeel::font (10.0f));
    voicesLabel.setColour (juce::Label::textColourId, Colours::textDim);
    voicesLabel.setJustificationType (juce::Justification::centredRight);

    presetButton.onClick = [this] { showPresetMenu(); };
    prev.onClick = [this] { proc.presets.loadNext (-1); };
    next.onClick = [this] { proc.presets.loadNext (1); };
    init.onClick = [this] { proc.presets.initPatch(); };
    save.onClick = [this] { savePreset(); };
    refreshPresetList();
}

void HeaderBar::refreshPresetList()
{
    proc.presets.refresh();
}

void HeaderBar::showPresetMenu()
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
    menu.addSeparator();
    menu.addItem (10001, "Open presets folder");
    menu.addItem (10002, "Delete current preset", proc.presets.getCurrentIndex() >= 0
                                                    && e[proc.presets.getCurrentIndex()].factoryIndex < 0);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetButton), [this] (int result)
    {
        if (result <= 0) return;
        if (result == 10001)
        {
            auto f = PresetManager::getPresetFolder();
            f.createDirectory();
            f.startAsProcess();
        }
        else if (result == 10002)
            proc.presets.deleteUserPreset (proc.presets.getCurrentIndex());
        else
            proc.presets.load (result - 1);
    });
}

void HeaderBar::savePreset()
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
        {
            const auto name = saveWindow->getTextEditorContents ("name");
            const auto cat = saveWindow->getTextEditorContents ("category");
            if (! proc.presets.save (name, cat))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Ferrite", "Could not save preset.");
        }
        saveWindow.reset();
    }), false);
}

void HeaderBar::tick()
{
    presetButton.setButtonText (proc.presets.getCurrentName());
    voicesLabel.setText ("voices " + juce::String (proc.activeVoices.load()) + "/" + juce::String (kNumVoices), juce::dontSendNotification);
}

void HeaderBar::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (Colours::outline);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);

    g.setColour (Colours::accent);
    g.setFont (FerriteLookAndFeel::font (24.0f, true));
    g.drawText ("FERRITE", juce::Rectangle<int> (14, 0, 130, getHeight() - 14), juce::Justification::centredLeft);
    g.setColour (Colours::textDim);
    g.setFont (FerriteLookAndFeel::font (10.0f));
    g.drawText ("SAMPLE FM", juce::Rectangle<int> (15, getHeight() - 24, 130, 14), juce::Justification::centredLeft);
}

void HeaderBar::resized()
{
    auto r = getLocalBounds().reduced (8, 6);
    r.removeFromLeft (130);

    auto presetArea = r.removeFromLeft (360).withSizeKeepingCentre (360, 28);
    prev.setBounds (presetArea.removeFromLeft (26));
    presetArea.removeFromLeft (4);
    init.setBounds (presetArea.removeFromRight (44));
    presetArea.removeFromRight (4);
    save.setBounds (presetArea.removeFromRight (50));
    presetArea.removeFromRight (4);
    next.setBounds (presetArea.removeFromRight (26));
    presetArea.removeFromRight (4);
    presetButton.setBounds (presetArea);

    r.removeFromLeft (16);
    auto scopeArea = r.removeFromRight (190);
    voicesLabel.setBounds (scopeArea.removeFromBottom (12));
    scope.setBounds (scopeArea);
    r.removeFromRight (10);
    polyMode.setBounds (r.removeFromLeft (84).withTrimmedBottom (8));
    r.removeFromLeft (8);
    layoutRow (r, { &glide, &bend, &uni, &uniDetune, &uniSpread, &fm, &master });
}

// ===========================================================================
FerriteEditor::FerriteEditor (FerriteProcessor& p)
    : AudioProcessorEditor (&p), proc (p), header (p), routing (p), detail (p),
      filter (p), lfo1 (p, 0), lfo2 (p, 1), modMatrix (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    content.addAndMakeVisible (header);
    for (int i = 0; i < kNumOps; ++i)
    {
        cards.push_back (std::make_unique<OpCard> (proc, i, [this] (int op) { selectOperator (op); }));
        content.addAndMakeVisible (*cards.back());
    }
    for (juce::Component* c : std::initializer_list<juce::Component*> { &routing, &detail, &filter, &lfo1, &lfo2, &modMatrix })
        content.addAndMakeVisible (c);
    routing.diagram.onOperatorClicked = [this] (int op) { selectOperator (op); };

    selectOperator (0);

    setResizable (true, true);
    setResizeLimits (kBaseW / 2, kBaseH / 2, kBaseW * 2, kBaseH * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) kBaseW / kBaseH);
    setSize (kBaseW, kBaseH);
    startTimerHz (30);
}

FerriteEditor::~FerriteEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void FerriteEditor::selectOperator (int op)
{
    for (int i = 0; i < kNumOps; ++i) cards[(size_t) i]->setSelected (i == op);
    detail.setOperator (op);
}

void FerriteEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::bg);
}

void FerriteEditor::resized()
{
    const float scale = (float) getWidth() / (float) kBaseW;
    content.setBounds (0, 0, kBaseW, kBaseH);
    content.setTransform (juce::AffineTransform::scale (scale));

    auto r = juce::Rectangle<int> (0, 0, kBaseW, kBaseH).reduced (8);
    header.setBounds (r.removeFromTop (60));
    r.removeFromTop (8);

    auto cardRow = r.removeFromTop (96);
    const int cw = (cardRow.getWidth() - 8 * (kNumOps - 1)) / kNumOps;
    for (auto& c : cards)
    {
        c->setBounds (cardRow.removeFromLeft (cw));
        cardRow.removeFromLeft (8);
    }
    r.removeFromTop (8);

    auto mid = r.removeFromTop (350);
    routing.setBounds (mid.removeFromLeft (460));
    mid.removeFromLeft (8);
    detail.setBounds (mid);
    r.removeFromTop (8);

    filter.setBounds (r.removeFromLeft (400));
    r.removeFromLeft (8);
    lfo1.setBounds (r.removeFromLeft (190));
    r.removeFromLeft (8);
    lfo2.setBounds (r.removeFromLeft (190));
    r.removeFromLeft (8);
    modMatrix.setBounds (r);
}

void FerriteEditor::timerCallback()
{
    // осциллограф
    const int ready = proc.scopeFifo.getNumReady();
    if (ready > 0)
    {
        const auto rd = proc.scopeFifo.read (ready);
        if (rd.blockSize1 > 0) header.scope.push (proc.scopeBuffer.data() + rd.startIndex1, rd.blockSize1);
        if (rd.blockSize2 > 0) header.scope.push (proc.scopeBuffer.data() + rd.startIndex2, rd.blockSize2);
        header.scope.repaint();
    }

    header.tick();
    routing.refresh();
    detail.refresh();
    lfo1.refresh();
    lfo2.refresh();
    for (auto& c : cards) c->refresh();
}
} // namespace ferrite
