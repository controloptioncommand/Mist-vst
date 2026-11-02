#include "SampleAsset.h"

namespace mist
{
float detectPitchMidi (const float* x, int n, double sr)
{
    // YIN по отрезку из «устойчивой» части семпла
    const int minTau = juce::jmax (2, (int) (sr / 2000.0));
    const int maxTau = (int) (sr / 30.0);
    const int W = juce::jmin (4096, n / 2);
    if (W < 256 || n < W + maxTau + 1) return -1.0f;

    // начинаем после атаки: 15% длины, но так, чтобы хватило окна
    int start = juce::jmin ((int) (n * 0.15), n - W - maxTau - 1);
    start = juce::jmax (0, start);
    const float* s = x + start;

    std::vector<float> d ((size_t) maxTau + 1, 0.0f);
    for (int tau = 1; tau <= maxTau; ++tau)
    {
        double acc = 0;
        for (int j = 0; j < W; ++j)
        {
            const float diff = s[j] - s[j + tau];
            acc += (double) diff * diff;
        }
        d[(size_t) tau] = (float) acc;
    }

    // cumulative mean normalised difference
    std::vector<float> cm ((size_t) maxTau + 1, 1.0f);
    double running = 0;
    for (int tau = 1; tau <= maxTau; ++tau)
    {
        running += d[(size_t) tau];
        cm[(size_t) tau] = running > 0 ? (float) (d[(size_t) tau] * tau / running) : 1.0f;
    }

    int best = -1;
    for (int tau = minTau; tau < maxTau; ++tau)
    {
        if (cm[(size_t) tau] < 0.15f)
        {
            while (tau + 1 < maxTau && cm[(size_t) tau + 1] < cm[(size_t) tau]) ++tau;
            best = tau;
            break;
        }
    }
    if (best < 0)
    {
        float m = 1.0f;
        for (int tau = minTau; tau < maxTau; ++tau)
            if (cm[(size_t) tau] < m) { m = cm[(size_t) tau]; best = tau; }
        if (m > 0.35f) return -1.0f;   // шум / непитчевый материал
    }

    // параболическая интерполяция
    double tauF = best;
    if (best > 1 && best < maxTau)
    {
        const double a = cm[(size_t) best - 1], b = cm[(size_t) best], c = cm[(size_t) best + 1];
        const double den = a - 2 * b + c;
        if (std::abs (den) > 1e-12) tauF = best + 0.5 * (a - c) / den;
    }
    const double hz = sr / tauF;
    return (float) (69.0 + 12.0 * std::log2 (hz / 440.0));
}

std::shared_ptr<SampleAsset> SampleAsset::fromReader (juce::AudioFormatReader& reader, const juce::String& name,
                                                      bool encode, juce::String& error)
{
    if (reader.sampleRate <= 0 || reader.lengthInSamples <= 0)
    {
        error = "Empty or invalid audio file";
        return {};
    }

    auto asset = std::make_shared<SampleAsset>();
    asset->name = name;
    asset->fileSampleRate = reader.sampleRate;

    const auto maxLen = (juce::int64) (kMaxSeconds * reader.sampleRate);
    const int len = (int) juce::jmin (reader.lengthInSamples, maxLen);
    const int chans = (int) juce::jmax (1u, reader.numChannels);

    juce::AudioBuffer<float> buf (chans, len);
    reader.read (&buf, 0, len, 0, true, true);

    asset->length = len;
    asset->audio.assign ((size_t) (len + kPadFront + kPadBack), 0.0f);
    float* dst = asset->audio.data() + kPadFront;
    for (int c = 0; c < chans; ++c)
        juce::FloatVectorOperations::addWithMultiply (dst, buf.getReadPointer (c), 1.0f / (float) chans, len);

    // нормализация пика к 0.98, удаление DC
    double mean = 0;
    for (int i = 0; i < len; ++i) mean += dst[i];
    mean /= juce::jmax (1, len);
    float peak = 0;
    for (int i = 0; i < len; ++i) { dst[i] -= (float) mean; peak = juce::jmax (peak, std::abs (dst[i])); }
    if (peak > 1.0e-6f)
        juce::FloatVectorOperations::multiply (dst, 0.98f / peak, len);

    if (encode)
    {
        auto* outStream = new juce::MemoryOutputStream (asset->encoded, false);
        std::unique_ptr<juce::OutputStream> os (outStream);
        juce::FlacAudioFormat flac;
        // FLAC не умеет нецелые/экзотические частоты — округляем, частоту храним отдельно
        auto writer = flac.createWriterFor (os, juce::AudioFormatWriterOptions{}
                                                     .withSampleRate (std::round (reader.sampleRate))
                                                     .withNumChannels (1)
                                                     .withBitsPerSample (24));
        if (writer != nullptr)
        {
            const float* ch[] = { dst };
            writer->writeFromFloatArrays (ch, 1, len);
            writer.reset();   // flush
        }
        else
        {
            error = "Could not encode sample";
        }
    }

    asset->analyse();
    return asset;
}

void SampleAsset::analyse()
{
    const float* x = samples();
    detectedMidi = detectPitchMidi (x, length, fileSampleRate);

    // период нарезки wavetable
    double period = detectedMidi > 0 ? fileSampleRate / (440.0 * std::pow (2.0, (detectedMidi - 69.0) / 12.0))
                                     : 1024.0;
    period = juce::jlimit (16.0, 8192.0, period);
    if (length < (int) (2 * period) + 4)
        period = juce::jmax (8.0, (length - 4) / 2.0);
    periodSamples = period;

    // Кадры берём только из слышимой части (выше −45 дБ от пика): тишина в начале и в хвосте
    // давала бы немые кадры, и Scrub в этих позициях молчал.
    int audibleStart = 0, audibleEnd = length;
    {
        float peak = 0.0f;
        for (int i = 0; i < length; ++i) peak = juce::jmax (peak, std::abs (x[i]));
        const float thr = peak * 0.0056f;
        while (audibleStart < length && std::abs (x[audibleStart]) < thr) ++audibleStart;
        while (audibleEnd > audibleStart && std::abs (x[audibleEnd - 1]) < thr) --audibleEnd;
        // слышимая часть короче двух периодов — берём весь семпл, как раньше
        if (audibleEnd - audibleStart < (int) (2 * period) + 4) { audibleStart = 0; audibleEnd = length; }
    }

    // кадры: окно 2*period, бесшовная склейка y(t) = t*x(p+tP) + (1-t)*x(p+P+tP)
    const int maxFrames = 64;
    const double usable = juce::jmax (0.0, (double) (audibleEnd - audibleStart) - 2.0 * period - 2.0);
    int numFrames = (int) juce::jlimit (1.0, (double) maxFrames, usable / period + 1.0);
    std::vector<std::vector<float>> frames ((size_t) numFrames, std::vector<float> ((size_t) Wavetable::N));
    for (int f = 0; f < numFrames; ++f)
    {
        const double pos = audibleStart + (numFrames > 1 ? usable * f / (numFrames - 1) : 0.0);
        auto& fr = frames[(size_t) f];
        for (int i = 0; i < Wavetable::N; ++i)
        {
            const double t = (double) i / Wavetable::N;
            const float a = readCubic (pos + t * period);
            const float b = readCubic (pos + period + t * period);
            fr[(size_t) i] = (float) (t * a + (1.0 - t) * b);
        }
    }
    table.buildFromFrames (frames);

    // обзор для отрисовки
    const int bins = 512;
    overview.assign ((size_t) bins * 2, 0.0f);
    for (int b = 0; b < bins; ++b)
    {
        const int s0 = (int) ((juce::int64) length * b / bins);
        const int s1 = juce::jmax (s0 + 1, (int) ((juce::int64) length * (b + 1) / bins));
        float mn = 0, mx = 0;
        for (int i = s0; i < juce::jmin (s1, length); ++i) { mn = juce::jmin (mn, x[i]); mx = juce::jmax (mx, x[i]); }
        overview[(size_t) b * 2] = mn;
        overview[(size_t) b * 2 + 1] = mx;
    }
}

std::shared_ptr<SampleAsset> SampleAsset::loadFromFile (const juce::File& file, juce::String& error)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr)
    {
        error = "Unsupported audio file: " + file.getFileName();
        return {};
    }
    return fromReader (*reader, file.getFileNameWithoutExtension(), true, error);
}

std::shared_ptr<SampleAsset> SampleAsset::loadFromEncoded (const juce::MemoryBlock& flacData, const juce::String& name, juce::String& error)
{
    juce::FlacAudioFormat flac;
    std::unique_ptr<juce::AudioFormatReader> reader (flac.createReaderFor (new juce::MemoryInputStream (flacData, true), true));
    if (reader == nullptr)
    {
        error = "Corrupted sample data: " + name;
        return {};
    }
    auto asset = fromReader (*reader, name, false, error);
    if (asset) asset->encoded = flacData;
    return asset;
}
} // namespace mist
