#pragma once
#include <juce_core/juce_core.h>
#include <cmath>

namespace loom
{
// ---------------------------------------------------------------------------
// Коэффициенты огибающей — считаются один раз на блок и общие для всех голосов
struct EnvCoefs
{
    int delaySamples = 0;
    int holdSamples = 0;
    float attackInc = 1.0f;     // линейный прирост за сэмпл
    float decayCoef = 0.0f;     // экспонента к sustain
    float sustain = 1.0f;
    float releaseCoef = 0.0f;

    void set (double sr, float d, float a, float h, float dec, float s, float r)
    {
        delaySamples = (int) (d * sr);
        holdSamples = (int) (h * sr);
        attackInc = a <= 0.0005f ? 1.0f : (float) (1.0 / (a * sr));
        decayCoef = dec <= 0.0005f ? 0.0f : (float) std::exp (-6.9 / (dec * sr));
        sustain = s;
        releaseCoef = r <= 0.0005f ? 0.0f : (float) std::exp (-6.9 / (r * sr));
    }
};

class Envelope
{
public:
    enum Stage { Idle, Delay, Attack, Hold, Decay, Sustain, Release };

    void noteOn (const EnvCoefs& c) noexcept
    {
        counter = 0;
        stage = c.delaySamples > 0 ? Delay : Attack;
    }

    void noteOff() noexcept
    {
        if (stage != Idle) stage = Release;
    }

    void reset() noexcept { stage = Idle; level = 0.0f; }
    bool isIdle() const noexcept { return stage == Idle; }
    float getLevel() const noexcept { return level; }
    Stage getStage() const noexcept { return stage; }

    inline float next (const EnvCoefs& c) noexcept
    {
        switch (stage)
        {
            case Idle: break;
            case Delay:
                if (++counter >= c.delaySamples) { stage = Attack; counter = 0; }
                break;
            case Attack:
                level += c.attackInc;
                if (level >= 1.0f) { level = 1.0f; stage = c.holdSamples > 0 ? Hold : Decay; counter = 0; }
                break;
            case Hold:
                if (++counter >= c.holdSamples) stage = Decay;
                break;
            case Decay:
                level = c.sustain + (level - c.sustain) * c.decayCoef;
                if (std::abs (level - c.sustain) < 1.0e-4f) { level = c.sustain; stage = Sustain; }
                break;
            case Sustain:
                // sustain может меняться ручкой — плавно идём к нему
                level += (c.sustain - level) * 0.002f;
                break;
            case Release:
                level *= c.releaseCoef;
                if (level < 1.0e-5f) { level = 0.0f; stage = Idle; }
                break;
        }
        return level;
    }

private:
    Stage stage = Idle;
    float level = 0.0f;
    int counter = 0;
};

// ---------------------------------------------------------------------------
// TPT State Variable Filter (Zavalishin / Cytomic), стерео
class SVF
{
public:
    void reset() noexcept { for (auto& s : st) s = 0.0f; }

    void setCoefs (float cutoffHz, float reso, double sr) noexcept
    {
        const float fc = juce::jlimit (10.0f, (float) (sr * 0.45), cutoffHz);
        g = (float) std::tan (juce::MathConstants<double>::pi * fc / sr);
        k = 2.0f - 1.96f * juce::jlimit (0.0f, 1.0f, reso);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
        // второй каскад для 24 dB — баттерворт
        k2 = 1.41421356f;
        b1 = 1.0f / (1.0f + g * (g + k2));
        b2 = g * b1;
        b3 = g * b2;
    }

    // type: 0 LP12, 1 LP24, 2 BP, 3 HP, 4 Notch; ch: 0/1
    inline float process (float v0, int type, int ch) noexcept
    {
        float* s = st + ch * 4;
        const float v3 = v0 - s[1];
        const float v1 = a1 * s[0] + a2 * v3;
        const float v2 = s[1] + a2 * s[0] + a3 * v3;
        s[0] = 2.0f * v1 - s[0];
        s[1] = 2.0f * v2 - s[1];

        switch (type)
        {
            case 0: return v2;
            case 1:
            {
                const float w3 = v2 - s[3];
                const float w1 = b1 * s[2] + b2 * w3;
                const float w2 = s[3] + b2 * s[2] + b3 * w3;
                s[2] = 2.0f * w1 - s[2];
                s[3] = 2.0f * w2 - s[3];
                return w2;
            }
            case 2: return v1 * k;            // нормированный BP
            case 3: return v0 - k * v1 - v2;
            default: return v0 - k * v1;      // notch = low + high
        }
    }

private:
    float g = 0, k = 2, a1 = 0, a2 = 0, a3 = 0, k2 = 1.414f, b1 = 0, b2 = 0, b3 = 0;
    float st[8] {};
};

inline float midiToHz (float note) noexcept { return 440.0f * std::exp2 ((note - 69.0f) / 12.0f); }

// быстрый тангенс-гиперболический сатуратор
inline float softClip (float x) noexcept
{
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}
} // namespace loom
