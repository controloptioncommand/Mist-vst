#include "Wavetable.h"

namespace mist
{
void Wavetable::buildFromSpectra (const std::vector<std::vector<std::complex<float>>>& spectra, bool normalisePerFrame)
{
    numFrames = (int) spectra.size();
    data.assign ((size_t) numFrames * kMips * kStride, 0.0f);

    juce::dsp::FFT fft (11);   // 2^11 = N
    static_assert (N == 2048, "FFT order mismatch");
    std::vector<float> buf (2 * N);

    for (int f = 0; f < numFrames; ++f)
    {
        const auto& spec = spectra[(size_t) f];
        float frameGain = 1.0f;

        for (int m = 0; m < kMips; ++m)
        {
            const int maxH = (N / 2) >> m;
            std::fill (buf.begin(), buf.end(), 0.0f);
            for (int h = 1; h <= maxH && h < (int) spec.size(); ++h)
            {
                // убираем гармонику Найквиста целиком (h == N/2), чтобы не было лишнего
                if (h == N / 2) continue;
                buf[(size_t) (2 * h)]     = spec[(size_t) h].real();
                buf[(size_t) (2 * h + 1)] = spec[(size_t) h].imag();
            }
            fft.performRealOnlyInverseTransform (buf.data());

            if (m == 0)
            {
                float peak = 0.0f;
                for (int i = 0; i < N; ++i) peak = juce::jmax (peak, std::abs (buf[(size_t) i]));
                frameGain = peak > 1.0e-9f ? 1.0f / peak : 0.0f;
            }

            float* dst = frame (f, m);
            for (int i = 0; i < N; ++i) dst[i] = buf[(size_t) i];
            dst[N] = dst[0];   // guard для интерполяции

            if (normalisePerFrame)
                for (int i = 0; i <= N; ++i) dst[i] *= frameGain;
        }
    }

    if (! normalisePerFrame)
    {
        // общая нормализация по самому громкому кадру (mip 0)
        float peak = 0.0f;
        for (int f = 0; f < numFrames; ++f)
        {
            auto* p = frame (f, 0);
            for (int i = 0; i < N; ++i) peak = juce::jmax (peak, std::abs (p[i]));
        }
        if (peak > 1.0e-9f)
            for (auto& v : data) v /= peak;
    }
}

void Wavetable::buildFromFrames (const std::vector<std::vector<float>>& frames)
{
    juce::dsp::FFT fft (11);
    std::vector<float> buf (2 * N);
    std::vector<std::vector<std::complex<float>>> spectra;
    spectra.reserve (frames.size());

    // мягкая нормализация: каждый кадр приводится к пику 1, но не усиливается больше чем в 20 раз
    // относительно самого громкого кадра (чтобы тишина не превращалась в шум)
    std::vector<float> rms (frames.size(), 0.0f);
    float maxRms = 0.0f;
    for (size_t f = 0; f < frames.size(); ++f)
    {
        double s = 0;
        for (float v : frames[f]) s += (double) v * v;
        rms[f] = (float) std::sqrt (s / N);
        maxRms = juce::jmax (maxRms, rms[f]);
    }

    for (size_t f = 0; f < frames.size(); ++f)
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        const float g = rms[f] > 1.0e-9f ? juce::jmin (1.0f / rms[f], 20.0f / juce::jmax (maxRms, 1.0e-9f)) : 0.0f;
        for (int i = 0; i < N; ++i) buf[(size_t) i] = frames[f][(size_t) i] * g;
        fft.performRealOnlyForwardTransform (buf.data(), true);
        std::vector<std::complex<float>> spec ((size_t) N / 2 + 1);
        for (int h = 1; h <= N / 2; ++h)
            spec[(size_t) h] = { buf[(size_t) (2 * h)], buf[(size_t) (2 * h + 1)] };
        spectra.push_back (std::move (spec));
    }

    // после выравнивания RMS — финальная нормализация по общему пику
    buildFromSpectra (spectra, false);
}

const Wavetable& Wavetable::getBasic()
{
    static const Wavetable table = []
    {
        Wavetable t;
        std::vector<std::vector<std::complex<float>>> spectra;
        const int H = N / 2;
        auto make = [&] (auto&& fn)
        {
            std::vector<std::complex<float>> s ((size_t) H + 1);
            for (int h = 1; h <= H; ++h) s[(size_t) h] = fn (h);
            spectra.push_back (std::move (s));
        };
        auto sinTerm = [] (float a) { return std::complex<float> (0.0f, -a); };
        auto cosTerm = [] (float a) { return std::complex<float> (a, 0.0f); };
        const float pi = juce::MathConstants<float>::pi;

        make ([&] (int h) { return h == 1 ? sinTerm (1.0f) : std::complex<float>(); });                              // Sine
        make ([&] (int h) { return h == 1 ? sinTerm (1.0f) : h == 2 ? sinTerm (0.5f) : std::complex<float>(); });     // Sine + oct
        make ([&] (int h) { return (h % 2) ? sinTerm (((h / 2) % 2 ? -1.0f : 1.0f) / (float) (h * h)) : std::complex<float>(); }); // Tri
        make ([&] (int h) { return (h % 2) ? sinTerm (1.0f / (float) h) : std::complex<float>(); });                  // Square
        make ([&] (int h) { return sinTerm ((h % 2 ? 1.0f : -1.0f) / (float) h); });                                  // Saw
        make ([&] (int h) { return cosTerm (std::sin (pi * (float) h * 0.25f) / (float) h); });                       // Pulse 25%
        make ([&] (int h) { return cosTerm (std::sin (pi * (float) h * 0.1f) / (float) h); });                        // Pulse 10%
        make ([&] (int h)                                                                                             // Formant-ish
        {
            const float c = 7.0f, w = 2.5f;
            const float a = std::exp (-0.5f * ((float) h - c) * ((float) h - c) / (w * w)) + 0.4f / (float) h;
            return sinTerm (a);
        });
        t.buildFromSpectra (spectra, true);
        return t;
    }();
    return table;
}
} // namespace mist
