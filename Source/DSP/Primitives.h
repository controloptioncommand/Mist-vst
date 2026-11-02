#pragma once
#include <juce_core/juce_core.h>
#include <cmath>

namespace mist
{
// ---------------------------------------------------------------------------
// Форма отрезка огибающей: f(p) = (1 − e^(−k·p)) / (1 − e^(−k)), p = 0…1.
// k = 0 — прямая, k > 0 — быстрый старт и плавный подход к цели, k < 0 — медленный старт.
// curve (−1…+1) → k = curve · kCurveSteepness. curve 0.5 даёт ту же экспоненту, что огибающие до v3.
struct EnvSegment
{
    double k = 0.0;        // крутизна
    double norm = 1.0;     // 1 / (1 − e^(−k))
    bool linear = true;

    void setCurve (float curve) noexcept
    {
        k = (double) juce::jlimit (-1.0f, 1.0f, curve) * 13.8;
        linear = std::abs (k) < 1.0e-3;
        norm = linear ? 1.0 : 1.0 / (1.0 - std::exp (-k));
    }

    // значение формы в точке p (для интерфейса)
    double shape (double p) const noexcept
    {
        p = juce::jlimit (0.0, 1.0, p);
        return linear ? p : (1.0 - std::exp (-k * p)) * norm;
    }
};

// Коэффициенты огибающей — считаются один раз на блок и общие для всех голосов
struct EnvCoefs
{
    int delaySamples = 0;
    int holdSamples = 0;
    double attackSamples = 0;   // длительность полной атаки 0 → 1 (в сэмплах), 0 — мгновенно
    double decayInc = 1.0;      // прирост фазы спада за сэмпл
    double releaseInc = 1.0;    // прирост фазы релиза за сэмпл
    float sustain = 1.0f;
    EnvSegment attack, decay, release;

    void set (double sr, float d, float a, float h, float dec, float s, float r,
              float aCurve = 0.0f, float dCurve = 0.5f, float rCurve = 0.5f)
    {
        delaySamples = (int) (d * sr);
        holdSamples = (int) (h * sr);
        attackSamples = a <= 0.0005f ? 0.0 : a * sr;
        decayInc = dec <= 0.0005f ? 1.0 : 1.0 / (dec * sr);
        sustain = s;
        releaseInc = r <= 0.0005f ? 1.0 : 1.0 / (r * sr);
        attack.setCurve (aCurve);
        decay.setCurve (dCurve);
        release.setCurve (rCurve);
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
        if (stage == Attack) beginAttack (c);
    }

    void noteOff() noexcept
    {
        if (stage == Idle) return;
        stage = Release;
        from = level;
        phase = 0.0;
        expState = 1.0;
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
                if (++counter >= c.delaySamples) { stage = Attack; counter = 0; beginAttack (c); }
                break;
            case Attack:
                if (attackInc <= 0.0 || (phase += attackInc) >= 1.0)
                {
                    level = 1.0f;
                    stage = c.holdSamples > 0 ? Hold : Decay;
                    counter = 0;
                    if (stage == Decay) beginFall();
                }
                else
                    level = (float) (from + (1.0 - from) * step (c.attack, attackInc));
                break;
            case Hold:
                if (++counter >= c.holdSamples) { stage = Decay; beginFall(); }
                break;
            case Decay:
                if ((phase += c.decayInc) >= 1.0) { level = c.sustain; stage = Sustain; }
                else level = (float) (1.0 - (1.0 - c.sustain) * step (c.decay, c.decayInc));
                break;
            case Sustain:
                // sustain может меняться ручкой — плавно идём к нему
                level += (c.sustain - level) * 0.002f;
                break;
            case Release:
                if ((phase += c.releaseInc) >= 1.0) { level = 0.0f; stage = Idle; }
                else level = (float) (from * (1.0 - step (c.release, c.releaseInc)));
                break;
        }
        return level;
    }

private:
    void beginAttack (const EnvCoefs& c) noexcept
    {
        // атака стартует с текущего уровня (повторное нажатие) и сохраняет скорость подъёма
        from = level;
        phase = 0.0;
        expState = 1.0;
        const double remaining = c.attackSamples * (1.0 - (double) from);
        attackInc = remaining < 1.0 ? 0.0 : 1.0 / remaining;
    }

    void beginFall() noexcept { phase = 0.0; expState = 1.0; }

    // значение формы после продвижения фазы на inc: e^(−k·p) ведём умножением (double — без накопления ошибки)
    inline double step (const EnvSegment& seg, double inc) noexcept
    {
        if (seg.linear) return phase;
        if (seg.k != lastK || inc != lastInc) { lastK = seg.k; lastInc = inc; mul = std::exp (-seg.k * inc); expState = std::exp (-seg.k * phase); }
        else expState *= mul;
        return (1.0 - expState) * seg.norm;
    }

    Stage stage = Idle;
    float level = 0.0f;
    int counter = 0;
    double from = 0.0, phase = 0.0, attackInc = 0.0, expState = 1.0;
    double lastK = 1.0e9, lastInc = -1.0, mul = 1.0;
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
} // namespace mist
