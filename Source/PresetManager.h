#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace mist
{
class MistProcessor;

// Пресеты: файлы *.mist (XML, семплы внутри в FLAC/base64) в ~/Music/Mist/Presets,
// плюс заводские пресеты, описанные в коде.
class PresetManager
{
public:
    explicit PresetManager (MistProcessor& p);

    struct Entry { juce::String name; juce::String category; juce::File file; int factoryIndex = -1; };

    static juce::File getPresetFolder();
    static constexpr const char* kExtension = ".mist";

    void refresh();
    const juce::Array<Entry>& getEntries() const { return entries; }

    bool load (int index);
    bool loadFile (const juce::File& f);
    bool save (const juce::String& name, const juce::String& category);
    bool deleteUserPreset (int index);
    void loadNext (int delta);
    void initPatch();

    juce::String getCurrentName() const { return currentName; }
    void setCurrentName (const juce::String& n) { currentName = n; }
    int getCurrentIndex() const { return currentIndex; }

private:
    MistProcessor& proc;
    juce::Array<Entry> entries;
    juce::String currentName { "Init" };
    int currentIndex = -1;
};
} // namespace mist
