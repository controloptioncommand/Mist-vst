#include "PresetManager.h"
#include "PluginProcessor.h"
#include "FactoryPresets.h"

namespace mist
{
PresetManager::PresetManager (MistProcessor& p) : proc (p) {}

juce::File PresetManager::getPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Mist").getChildFile ("Presets");
}

void PresetManager::refresh()
{
    entries.clear();
    const auto& factory = getFactoryPresets();
    for (int i = 0; i < (int) factory.size(); ++i)
        entries.add ({ factory[(size_t) i].name, "Factory", {}, i });

    auto folder = getPresetFolder();
    if (folder.isDirectory())
    {
        auto files = folder.findChildFiles (juce::File::findFiles, true, juce::String ("*") + kExtension);
        files.sort();
        for (auto& f : files)
        {
            auto rel = f.getParentDirectory().getRelativePathFrom (folder);
            entries.add ({ f.getFileNameWithoutExtension(), rel == "." ? "User" : rel, f, -1 });
        }
    }

    currentIndex = -1;
    for (int i = 0; i < entries.size(); ++i)
        if (entries[i].name == currentName) { currentIndex = i; break; }
}

bool PresetManager::load (int index)
{
    if (! juce::isPositiveAndBelow (index, entries.size())) return false;
    const auto& e = entries.getReference (index);

    if (e.factoryIndex >= 0)
    {
        proc.resetToDefaults();
        const auto& fp = getFactoryPresets()[(size_t) e.factoryIndex];
        for (const auto& [id, value] : fp.values)
            if (auto* p = proc.apvts.getParameter (id))
                p->setValueNotifyingHost (p->convertTo0to1 (value));
    }
    else
    {
        auto xml = juce::XmlDocument::parse (e.file);
        if (xml == nullptr) return false;
        proc.restoreStateXml (*xml);
    }
    currentName = e.name;
    currentIndex = index;
    return true;
}

bool PresetManager::loadFile (const juce::File& f)
{
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr) return false;
    proc.restoreStateXml (*xml);
    currentName = f.getFileNameWithoutExtension();
    refresh();
    return true;
}

bool PresetManager::save (const juce::String& rawName, const juce::String& category)
{
    const auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty()) return false;
    auto folder = getPresetFolder();
    if (category.isNotEmpty() && category != "User" && category != "Factory")
        folder = folder.getChildFile (juce::File::createLegalFileName (category));
    if (! folder.createDirectory()) return false;

    auto xml = proc.createStateXml (true);
    auto file = folder.getChildFile (name + kExtension);
    if (! xml->writeTo (file)) return false;

    currentName = name;
    refresh();
    return true;
}

bool PresetManager::deleteUserPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, entries.size())) return false;
    const auto& e = entries.getReference (index);
    if (e.factoryIndex >= 0) return false;
    const bool ok = e.file.deleteFile();
    refresh();
    return ok;
}

void PresetManager::loadNext (int delta)
{
    if (entries.isEmpty()) refresh();
    if (entries.isEmpty()) return;
    int idx = currentIndex < 0 ? 0 : (currentIndex + delta + entries.size()) % entries.size();
    load (idx);
}

void PresetManager::initPatch()
{
    proc.resetToDefaults();
    currentName = "Init";
    currentIndex = -1;
}
} // namespace mist
