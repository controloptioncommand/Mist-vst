#pragma once
#include "Wavetable.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <memory>

namespace ferrite
{
// Загруженный семпл оператора: моно-аудио, найденный питч, wavetable из него,
// и сжатая (FLAC) копия для сохранения внутри пресета/проекта.
struct SampleAsset
{
    juce::String name;
    double fileSampleRate = 44100.0;
    std::vector<float> audio;          // с отступами: kPadFront сэмплов в начале и kPadBack в конце
    int length = 0;                    // число реальных сэмплов
    float detectedMidi = -1.0f;        // -1 = питч не найден
    double periodSamples = 0.0;        // период, по которому нарезан wavetable
    Wavetable table;
    juce::MemoryBlock encoded;         // FLAC 24 bit
    std::vector<float> overview;       // пики для отрисовки (min/max парами)

    static constexpr int kPadFront = 4, kPadBack = 4;
    static constexpr double kMaxSeconds = 60.0;

    const float* samples() const noexcept { return audio.data() + kPadFront; }

    // Кубическая Эрмитова интерполяция; pos в сэмплах файла, 0..length
    inline float readCubic (double pos) const noexcept
    {
        const int i = (int) std::floor (pos);
        const float t = (float) (pos - i);
        const float* p = audio.data() + kPadFront + i;
        if (i < -1 || i > length + 1) return 0.0f;
        const float xm1 = p[-1], x0 = p[0], x1 = p[1], x2 = p[2];
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * t + c2) * t + c1) * t + x0;
    }

    static std::shared_ptr<SampleAsset> loadFromFile (const juce::File& file, juce::String& error);
    static std::shared_ptr<SampleAsset> loadFromEncoded (const juce::MemoryBlock& flac, const juce::String& name, juce::String& error);

private:
    static std::shared_ptr<SampleAsset> fromReader (juce::AudioFormatReader& reader, const juce::String& name,
                                                    bool encode, juce::String& error);
    void analyse();
};

float detectPitchMidi (const float* x, int n, double sampleRate);   // -1 если не найден
} // namespace ferrite
