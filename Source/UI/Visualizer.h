#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "LookAndFeel.h"
#include <deque>

namespace loom
{
// Визуализация результата. Три режима:
//  Waves    — «стопка» осциллограмм (как в макете), свежая — синяя, спереди
//  Spectrum — спектрограмма + текущий спектр (в духе MiniMeters)
//  Scope    — прокручивающаяся форма волны, окрашенная по частотам + стереометр
class Visualizer : public juce::Component
{
public:
    enum Mode { Waves = 0, Spectrum, Scope };

    Visualizer();
    void setSampleRate (double sr) { sampleRate = sr > 0 ? sr : 44100.0; }
    void push (const float* l, const float* r, int n);   // с UI-таймера
    void tick();                                          // 30 Гц
    void setMode (Mode m);
    Mode getMode() const { return mode; }

    void setDroneState (bool on, int lowestNote) { droneOn = on; droneNote = lowestNote; }
    std::function<void()> onDroneClicked;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;

private:
    void paintWaves (juce::Graphics& g, juce::Rectangle<float> area);
    void paintSpectrum (juce::Graphics& g, juce::Rectangle<float> area);
    void paintScope (juce::Graphics& g, juce::Rectangle<float> area);
    void paintOverlay (juce::Graphics& g);

    void captureWave();
    void computeSpectrumColumns();
    void computeScopeColumns();
    static juce::Colour magma (float t);

    double sampleRate = 44100.0;
    Mode mode = Waves;
    bool droneOn = false;
    int droneNote = -1;

    // кольцевой буфер входа
    static constexpr int kRing = 1 << 15;
    std::vector<float> ringL = std::vector<float> (kRing, 0.0f), ringR = std::vector<float> (kRing, 0.0f);
    juce::int64 written = 0;
    juce::int64 spectrumRead = 0, scopeRead = 0;

    // Waves
    static constexpr int kWavePoints = 220, kWaveLines = 26;
    std::deque<std::vector<float>> waveLines;
    int tickCount = 0;
    float waveGain = 1.0f;

    // Spectrum
    static constexpr int kFftOrder = 12, kFftSize = 1 << kFftOrder, kHop = 512;
    juce::dsp::FFT fft { kFftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) kFftSize, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> fftBuf = std::vector<float> (kFftSize * 2, 0.0f);
    std::vector<float> lastSpectrum;   // по пикселям высоты, 0..1
    std::vector<float> peakSpectrum;
    juce::Image spectrogram;

    // Scope
    struct ScopeCol { float mn = 0, mx = 0; juce::Colour c; };
    std::deque<ScopeCol> scopeCols;
    float lpLow = 0, lpMid = 0, bandState[3] {};
    std::vector<juce::Point<float>> stereoPts;

    juce::Rectangle<int> pillWaves, pillSpectrum, pillScope, pillDrone;
};
} // namespace loom
