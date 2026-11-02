#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "LookAndFeel.h"
#include <deque>

namespace mist
{
// Главный вид. Три вкладки (стеклянные, снизу справа):
//  Scope    — плотная волна: одна полоска на колонку пикселей, цвет по громкости (рампа несущей)
//  Waves    — хребтовые линии (ridgelines)
//  Spectrum — спектрограмма в той же рампе
// Сверху справа — стеклянная легенда «−24 dB [шкала] 0 dB».
class Visualizer : public juce::Component
{
public:
    enum Mode { Waves = 0, Spectrum, Scope };

    Visualizer();
    void setSampleRate (double sr) { sampleRate = sr > 0 ? sr : 44100.0; }
    void push (const float* l, const float* r, int n);
    void tick();
    void setMode (Mode m);
    Mode getMode() const { return mode; }

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    bool keyPressed (const juce::KeyPress& k) override;
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { repaint(); }

private:
    juce::Colour background() const;
    void paintScope (juce::Graphics& g, juce::Rectangle<float> area);
    void paintWaves (juce::Graphics& g, juce::Rectangle<float> area);
    void paintSpectrum (juce::Graphics& g, juce::Rectangle<float> area);
    void paintGuides (juce::Graphics& g, juce::Rectangle<float> area);
    void paintOverlay (juce::Graphics& g);

    void captureWave();
    void computeSpectrumColumns();
    void computeScopeColumns();

    double sampleRate = 44100.0;
    Mode mode = Scope;

    static constexpr int kRing = 1 << 15;
    std::vector<float> ringL = std::vector<float> (kRing, 0.0f), ringR = std::vector<float> (kRing, 0.0f);
    juce::int64 written = 0, spectrumRead = 0, scopeRead = 0;

    // Scope
    struct Col { float mn = 0, mx = 0, db = -100.0f; };
    std::deque<Col> scopeCols;

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
    static constexpr int kSpecColW = 2;
    std::deque<std::vector<juce::uint8>> spectroCols;   // история столбцов (значения 0..255 — тема может смениться)
    juce::Image specImage;
    int specRows = 0;
    double specRate = 0.0;
    std::vector<std::pair<int, int>> binRange;
    std::vector<float> tiltDb;
    std::array<juce::PixelARGB, 256> lut {};
    juce::String lutKey;
    void prepareSpectrum();
    void updateLut();
    void drawSpectrumColumn (int x, const std::vector<juce::uint8>& col);

    juce::Rectangle<float> tabsArea, legendArea;
};
} // namespace mist
