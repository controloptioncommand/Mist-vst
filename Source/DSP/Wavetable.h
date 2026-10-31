#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <complex>

namespace ferrite
{
// Band-limited wavetable: несколько кадров (frames), у каждого — mip-уровни,
// где уровень k содержит гармоники 1 .. (N/2 >> k). Чтение с линейной интерполяцией
// по фазе и между соседними кадрами (scan).
class Wavetable
{
public:
    static constexpr int N     = 2048;
    static constexpr int kMips = 11;   // 1024, 512, ... 1 гармоника
    static constexpr int kStride = N + 1;

    int getNumFrames() const noexcept { return numFrames; }

    // spectra[f][h] — комплексная амплитуда гармоники h (h=1..N/2) кадра f; [0] игнорируется
    void buildFromSpectra (const std::vector<std::vector<std::complex<float>>>& spectra, bool normalisePerFrame);
    // frames — N сэмплов на кадр, уже периодичные
    void buildFromFrames (const std::vector<std::vector<float>>& frames);

    static int mipForIncrement (double cyclesPerSample) noexcept
    {
        const double maxH = 0.5 / juce::jmax (cyclesPerSample, 1.0e-9);
        int k = 0;
        while (k < kMips - 1 && (double) ((N / 2) >> k) > maxH) ++k;
        return k;
    }

    // phase — в циклах (любое число), scan 0..1
    inline float read (double phase, float scan, int mip) const noexcept
    {
        const double ph = phase - std::floor (phase);
        const double fi = ph * N;
        const int i0 = (int) fi;
        const float t = (float) (fi - i0);

        if (numFrames <= 1)
        {
            const float* a = frame (0, mip);
            return a[i0] + t * (a[i0 + 1] - a[i0]);
        }

        const float fpos = scan * (float) (numFrames - 1);
        const int f0 = juce::jmin ((int) fpos, numFrames - 2);
        const float ft = fpos - (float) f0;
        const float* a = frame (f0, mip);
        const float* b = frame (f0 + 1, mip);
        const float va = a[i0] + t * (a[i0 + 1] - a[i0]);
        const float vb = b[i0] + t * (b[i0 + 1] - b[i0]);
        return va + ft * (vb - va);
    }

    const float* frame (int f, int mip) const noexcept { return data.data() + ((size_t) f * kMips + (size_t) mip) * kStride; }

    // Кадры для заданного scan/mip — считаются раз на подблок, чтение потом дешёвое
    struct Cursor { const float* a = nullptr; const float* b = nullptr; float ft = 0.0f; };
    Cursor cursor (float scan, int mip) const noexcept
    {
        Cursor c;
        if (numFrames <= 1) { c.a = frame (0, mip); return c; }
        const float fpos = juce::jlimit (0.0f, 1.0f, scan) * (float) (numFrames - 1);
        const int f0 = juce::jmin ((int) fpos, numFrames - 2);
        c.ft = fpos - (float) f0;
        c.a = frame (f0, mip);
        if (c.ft > 1.0e-5f) c.b = frame (f0 + 1, mip);
        return c;
    }

    static inline float read (double phase, const Cursor& c) noexcept
    {
        const double ph = phase - std::floor (phase);
        const double fi = ph * N;
        const int i0 = (int) fi;
        const float t = (float) (fi - i0);
        const float va = c.a[i0] + t * (c.a[i0 + 1] - c.a[i0]);
        if (c.b == nullptr) return va;
        const float vb = c.b[i0] + t * (c.b[i0 + 1] - c.b[i0]);
        return va + c.ft * (vb - va);
    }

    static const Wavetable& getBasic();   // Sine → Tri → Saw → Square → Pulse ...

private:
    float* frame (int f, int mip) noexcept { return data.data() + ((size_t) f * kMips + (size_t) mip) * kStride; }
    int numFrames = 0;
    std::vector<float> data;
};
} // namespace ferrite
