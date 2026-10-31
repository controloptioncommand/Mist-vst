#include "Synth.h"

namespace loom
{
// глубина фазовой модуляции (в циклах) при Level=100% и FM Amount=100%
static constexpr float kModScale = 1.5f;
// максимальная глубина обратной связи (в циклах)
static constexpr float kFeedbackScale = 0.45f;

// ===========================================================================
// LFO
float Lfo::shapeValue (int shape, double ph) noexcept
{
    const float p = (float) ph;
    switch (shape)
    {
        case LfoSine:      return std::sin (juce::MathConstants<float>::twoPi * p);
        case LfoTriangle:  return p < 0.5f ? 4.0f * p - 1.0f : 3.0f - 4.0f * p;
        case LfoSawUp:     return 2.0f * p - 1.0f;
        case LfoSawDown:   return 1.0f - 2.0f * p;
        case LfoSquare:    return p < 0.5f ? 1.0f : -1.0f;
        case LfoSampleHold:return held;
        case LfoSmoothRandom:
        {
            const float t = p * p * (3.0f - 2.0f * p);
            return prevHeld + (held - prevHeld) * t;
        }
        default: return 0.0f;
    }
}

void Lfo::process (const Snapshot::LfoSettings& s, const BlockInfo& info, int numSamples, std::vector<float>& out)
{
    const int numSub = (numSamples + kSubBlock - 1) / kSubBlock;
    out.resize ((size_t) juce::jmax (1, numSub));

    double cyclesPerSample = s.rateHz / sampleRate;
    if (s.sync)
    {
        const double beats = lfoDivisionInBeats (s.div);
        cyclesPerSample = (info.bpm / 60.0) / beats / sampleRate;
        if (info.playing && info.hasPpq)
        {
            // фаза жёстко привязана к позиции транспорта
            const double newPhase = std::fmod (info.ppq / beats, 1.0);
            if (newPhase < phase - 0.5) { prevHeld = held; held = rng.nextFloat() * 2.0f - 1.0f; }
            phase = newPhase < 0 ? newPhase + 1.0 : newPhase;
        }
    }

    for (int k = 0; k < numSub; ++k)
    {
        out[(size_t) k] = shapeValue (s.shape, phase);
        const int n = juce::jmin (kSubBlock, numSamples - k * kSubBlock);
        phase += cyclesPerSample * n;
        while (phase >= 1.0)
        {
            phase -= 1.0;
            prevHeld = held;
            held = rng.nextFloat() * 2.0f - 1.0f;
        }
    }
    lastValue = out[0];
}

// ===========================================================================
// Voice
void Voice::prepare (double sr)
{
    sampleRate = sr;
    kill();
}

void Voice::kill()
{
    active = false;
    releasing = false;
    held = sustained = false;
    stealLeft = 0;
    pendingNote = -1;
    for (auto& e : env) e.reset();
    modEnv.reset();
    filter.reset();
}

float Voice::getLoudness() const noexcept
{
    float m = 0;
    for (auto& e : env) m = juce::jmax (m, e.getLevel());
    return m;
}

void Voice::initSampleState (int u, int i, const OpSnap& o) noexcept
{
    sdone[u][i] = false;
    grainTimer[u][i] = 0.0f;
    for (auto& g : grains[u][i]) g.on = false;
    if (o.asset == nullptr) { spos[u][i] = 0; sdir[u][i] = 1; return; }
    const double len = o.asset->length;
    if (o.reverse) { spos[u][i] = (len - 1.0) * (1.0 - o.start); sdir[u][i] = -1; }
    else           { spos[u][i] = len * o.start;                 sdir[u][i] = 1; }
}

void Voice::start (int newNote, float vel, float glideFrom, const Snapshot& s, bool resetPhases)
{
    const bool wasActive = active;
    note = newNote;
    velocity = vel;
    active = true;
    releasing = false;
    held = true;
    sustained = false;
    stealLeft = 0;
    pendingNote = -1;
    randomValue = rng.nextFloat() * 2.0f - 1.0f;

    targetPitch = (float) newNote;
    if (s.glideSec > 0.0005f && glideFrom >= 0.0f)
    {
        pitch = glideFrom;
        glideStep = std::abs (targetPitch - pitch) / (float) (s.glideSec * sampleRate);
    }
    else
    {
        pitch = targetPitch;
        glideStep = 0;
    }

    for (int i = 0; i < kNumOps; ++i)
    {
        env[(size_t) i].noteOn (s.op[(size_t) i].env);
        if (! wasActive) prevAmp[(size_t) i] = 0.0f;
    }
    modEnv.noteOn (s.modEnv);

    if (resetPhases || ! wasActive)
    {
        filter.reset();
        for (int u = 0; u < kMaxUnison; ++u)
            for (int i = 0; i < kNumOps; ++i)
            {
                // при унисоне — случайные фазы, иначе одинаковая атака на каждой ноте
                phase[u][i] = s.uniVoices > 1 ? rng.nextDouble() : 0.0;
                out[u][i] = fb1[u][i] = fb2[u][i] = 0.0f;
                initSampleState (u, i, s.op[(size_t) i]);
            }
    }
    else
    {
        for (int u = 0; u < kMaxUnison; ++u)
            for (int i = 0; i < kNumOps; ++i)
                initSampleState (u, i, s.op[(size_t) i]);
    }
    for (int i = 0; i < kNumOps; ++i)
        lastAssetLength[i] = s.op[(size_t) i].asset ? s.op[(size_t) i].asset->length : 0;
}

void Voice::legatoTo (int newNote, const Snapshot& s)
{
    note = newNote;
    targetPitch = (float) newNote;
    held = true;
    if (s.glideSec > 0.0005f)
        glideStep = std::abs (targetPitch - pitch) / (float) (s.glideSec * sampleRate);
    else
    {
        pitch = targetPitch;
        glideStep = 0;
    }
}

void Voice::release()
{
    if (! active) return;
    held = false;
    sustained = false;
    releasing = true;
    for (auto& e : env) e.noteOff();
    modEnv.noteOff();
}

void Voice::beginSteal (int newNote, float newVel, float glideFrom)
{
    pendingNote = newNote;
    pendingVel = newVel;
    pendingGlide = glideFrom;
    stealLeft = kStealLen;
    held = false;
}

inline float Voice::readSample (int u, int i, const OpSnap& o, double pmCycles) noexcept
{
    const SampleAsset& a = *o.asset;
    if (sdone[u][i]) return 0.0f;

    const double len = a.length;
    const double period = a.fileSampleRate / midiToHz ((float) o.root);
    double rp = spos[u][i] + pmCycles * period;

    double ls = o.loopStart * len, le = o.loopEnd * len;
    if (le - ls < 16.0) le = juce::jmin (len, ls + 16.0);

    // кроссфейд в конце петли (только для прямого направления)
    if (o.loopMode == LoopForward && o.xfade > 0.0f && sdir[u][i] > 0)
    {
        const double xf = juce::jmin ((double) o.xfade * (le - ls), ls);
        if (xf > 1.0 && rp > le - xf && rp < le)
        {
            const float t = (float) ((rp - (le - xf)) / xf);
            auto wrap = [len] (double p) { p = std::fmod (p, len); return p < 0 ? p + len : p; };
            const float va = a.read (wrap (rp), hermiteRead);
            const float vb = a.read (wrap (rp - (le - ls)), hermiteRead);
            // равномощный кроссфейд
            return va * std::sqrt (1.0f - t) + vb * std::sqrt (t);
        }
    }

    rp = std::fmod (rp, len);
    if (rp < 0) rp += len;
    return a.read (rp, hermiteRead);
}

inline float Voice::renderGrains (int u, int i, const OpSnap& o, double pmCycles, double inc, float scanPos, bool hermite) noexcept
{
    const SampleAsset& a = *o.asset;
    const double len = a.length;
    const float grainLen = juce::jmax (16.0f, o.grainSize * (float) sampleRate);
    const float density = juce::jlimit (1.0f, (float) kMaxGrains, o.grainDensity);

    // запуск нового зерна
    float& timer = grainTimer[u][i];
    timer -= 1.0f;
    if (timer <= 0.0f)
    {
        for (auto& g : grains[u][i])
            if (! g.on)
            {
                const float spray = o.grainSpray * (rng.nextFloat() - 0.5f);
                const double p = juce::jlimit (0.0, len - 1.0, (double) (scanPos + spray) * len);
                g.pos = p;
                g.age = 0.0f;
                g.len = grainLen;
                g.dir = o.reverse ? -1 : 1;
                g.on = true;
                break;
            }
        timer += grainLen / density * (1.0f + o.grainSpray * 0.5f * (rng.nextFloat() - 0.5f));
    }

    const double period = a.fileSampleRate / midiToHz ((float) o.root);
    const double offset = pmCycles * period;
    float sum = 0.0f;
    for (auto& g : grains[u][i])
    {
        if (! g.on) continue;
        const float t = g.age / g.len;
        const float w = 4.0f * t * (1.0f - t);          // параболическое окно
        double rp = std::fmod (g.pos + offset, len);
        if (rp < 0) rp += len;
        sum += w * a.read (rp, hermite);
        g.pos += inc * g.dir;
        if (g.pos >= len) g.pos -= len;
        else if (g.pos < 0) g.pos += len;
        g.age += 1.0f;
        if (g.age >= g.len) g.on = false;
    }
    // сумма перекрывающихся парабол ≈ density * 2/3
    return sum / juce::jmax (1.0f, density * 0.6667f);
}

inline void Voice::advanceSample (int u, int i, const OpSnap& o, double inc) noexcept
{
    if (sdone[u][i]) return;
    const double len = o.asset->length;
    double& p = spos[u][i];
    int& dir = sdir[u][i];
    p += inc * dir;

    if (o.loopMode == LoopOff)
    {
        if (p >= len || p < 0) sdone[u][i] = true;
        return;
    }

    double ls = o.loopStart * len, le = o.loopEnd * len;
    if (le - ls < 16.0) le = juce::jmin (len, ls + 16.0);
    const double ll = juce::jmax (1.0, le - ls);

    if (o.loopMode == LoopForward)
    {
        if (dir > 0 && p >= le)      p = ls + std::fmod (p - le, ll);
        else if (dir < 0 && p < ls)  p = le - std::fmod (ls - p, ll);
    }
    else
    {
        if (dir > 0 && p >= le)      { p = juce::jmax (ls, 2.0 * le - p); dir = -1; }
        else if (dir < 0 && p < ls)  { p = juce::jmin (le, 2.0 * ls - p); dir = 1; }
    }
    if (p >= len || p < 0) p = juce::jlimit (0.0, len - 1.0, p);
}

void Voice::render (float* L, float* R, int absStart, int numSamples, const BlockContext& ctx)
{
    int pos = 0;
    while (pos < numSamples && active)
    {
        const int abs = absStart + pos;
        const int toBoundary = kSubBlock - (abs % kSubBlock);
        const int n = juce::jmin (toBoundary, numSamples - pos);
        renderChunk (L + pos, R + pos, n, abs / kSubBlock, ctx);
        pos += n;
    }
}

void Voice::renderChunk (float* L, float* R, int n, int lfoIndex, const BlockContext& ctx)
{
    const Snapshot& s = *ctx.snap;
    const double sr = ctx.sampleRate;

    // ------------------------------------------------ модуляционная матрица
    float dst[NumModDests] {};
    if (s.anyModulation)
    {
        float src[NumModSources] {};
        src[SrcLfo1]       = ctx.lfo[0][lfoIndex];
        src[SrcLfo2]       = ctx.lfo[1][lfoIndex];
        src[SrcModEnv]     = modEnv.getLevel();
        src[SrcVelocity]   = velocity;
        src[SrcKeyTrack]   = ((float) note - 60.0f) / 60.0f;
        src[SrcModWheel]   = ctx.modWheel;
        src[SrcAftertouch] = ctx.aftertouch;
        src[SrcRandom]     = randomValue;
        for (auto& sl : s.slots)
            if (sl.src > 0 && sl.dst > 0)
                dst[sl.dst] += src[sl.src] * sl.amt;
    }

    // ------------------------------------------------ глайд
    if (glideStep > 0.0f)
    {
        const float step = glideStep * (float) n;
        if (std::abs (targetPitch - pitch) <= step) { pitch = targetPitch; glideStep = 0; }
        else pitch += (targetPitch > pitch ? step : -step);
    }
    const float basePitch = pitch + ctx.bendSemis + dst[DstPitch] * 24.0f;
    const float baseHz = midiToHz (basePitch);
    const float fmAmt = juce::jlimit (0.0f, 4.0f, s.fmAmount + dst[DstFMAmount] * 2.0f) * kModScale;

    // ------------------------------------------------ унисон
    const int U = juce::jlimit (1, kMaxUnison, s.uniVoices);
    const float uniDet = s.uniDetune * juce::jlimit (0.0f, 3.0f, 1.0f + dst[DstUniDetune]);
    float uniMul[kMaxUnison], uniPan[kMaxUnison];
    for (int u = 0; u < U; ++u)
    {
        const float sp = U > 1 ? (2.0f * (float) u / (float) (U - 1) - 1.0f) : 0.0f;
        uniMul[u] = std::exp2 (uniDet * sp / 1200.0f);
        // чередуем панораму, чтобы соседние по детюну голоса расходились
        uniPan[u] = (u % 2 ? -1.0f : 1.0f) * std::abs (sp) * s.uniSpread;
    }
    const float uniNorm = 1.0f / std::sqrt ((float) U);

    // ------------------------------------------------ операторы
    bool opActive[kNumOps];
    float amp[kNumOps], scan[kNumOps], fbAmt[kNumOps];
    double inc[kNumOps];
    int mip[kNumOps];
    Wavetable::Cursor cur[kNumOps];
    float gL[kMaxUnison][kNumOps], gR[kMaxUnison][kNumOps];
    bool useSample[kNumOps], useGrain[kNumOps];
    hermiteRead = s.hermite;

    for (int i = 0; i < kNumOps; ++i)
    {
        const OpSnap& o = s.op[(size_t) i];
        opActive[i] = o.on && ! env[(size_t) i].isIdle();
        useSample[i] = (o.mode == OpModeSample || o.mode == OpModeGrain) && o.asset != nullptr;
        useGrain[i] = o.mode == OpModeGrain && o.asset != nullptr;
        if (! opActive[i]) { amp[i] = 0; continue; }

        if (useSample[i] && o.asset->length != lastAssetLength[i])
        {
            // семпл заменили во время звучания ноты
            for (int u = 0; u < kMaxUnison; ++u) initSampleState (u, i, o);
            lastAssetLength[i] = o.asset->length;
        }

        const float opPitchMul = std::exp2 (dst[DstOpPitch0 + i] * 24.0f / 12.0f);
        const float lv = juce::jlimit (0.0f, 1.0f, o.level + dst[DstOpLevel0 + i]);
        const float velGain = 1.0f - o.velSens * (1.0f - velocity);
        amp[i] = lv * lv * velGain;
        scan[i] = juce::jlimit (0.0f, 1.0f, o.scan + dst[DstOpScan0 + i]);
        const float fb = juce::jlimit (0.0f, 1.0f, o.feedback + dst[DstOpFeedback0 + i]);
        fbAmt[i] = fb * fb * kFeedbackScale;

        if (useSample[i])
        {
            const float rootHz = midiToHz ((float) o.root);
            float hz;
            if (o.fixed)            hz = o.fixedHz;
            else if (o.keytrack)    hz = baseHz * o.ratio;
            else                    hz = rootHz * o.ratio;
            hz *= o.detuneMul * opPitchMul;
            inc[i] = (double) hz / rootHz * o.asset->fileSampleRate / sr;   // сэмплов файла за сэмпл
            mip[i] = 0;
        }
        else
        {
            const float hz = (o.fixed ? o.fixedHz : baseHz * o.ratio) * o.detuneMul * opPitchMul;
            inc[i] = hz / sr;
            mip[i] = Wavetable::mipForIncrement (inc[i] * (U > 1 ? std::exp2 (uniDet / 1200.0f) : 1.0f));
            cur[i] = o.table->cursor (scan[i], mip[i]);
        }

        if (s.isCarrier[i])
        {
            const float g = s.outGain[i] * s.carrierNorm * uniNorm;
            for (int u = 0; u < U; ++u)
            {
                const float pan = juce::jlimit (-1.0f, 1.0f, o.pan + uniPan[u] + dst[DstPan]);
                const float a = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
                gL[u][i] = g * std::cos (a) * 1.41421356f;
                gR[u][i] = g * std::sin (a) * 1.41421356f;
            }
        }
    }

    // ------------------------------------------------ фильтр
    const bool filterOn = s.filterOn;
    if (filterOn)
    {
        const float oct = s.filterEnv * modEnv.getLevel() * 6.0f + dst[DstCutoff] * 6.0f
                        + s.filterKey * (pitch - 60.0f) / 12.0f;
        filter.setCoefs (s.cutoff * std::exp2 (oct), juce::jlimit (0.0f, 1.0f, s.reso + dst[DstReso]), sr);
    }
    const float driveGain = 1.0f + s.drive * 8.0f;
    const float driveComp = 1.0f / (1.0f + s.drive * 2.0f);
    const float volume = juce::jlimit (0.0f, 2.0f, 1.0f + dst[DstVolume]) * s.masterGain;

    // ------------------------------------------------ per-sample
    float envVal[kNumOps];
    const float invN = 1.0f / (float) n;

    for (int k = 0; k < n; ++k)
    {
        float g[kNumOps];
        for (int i = 0; i < kNumOps; ++i)
        {
            if (! opActive[i]) { g[i] = 0; continue; }
            envVal[i] = env[(size_t) i].next (s.op[(size_t) i].env);
            const float a = prevAmp[i] + (amp[i] - prevAmp[i]) * (float) (k + 1) * invN;
            g[i] = envVal[i] * a;
        }

        float sumL = 0, sumR = 0;
        for (int u = 0; u < U; ++u)
        {
            float* o = out[u];
            for (int i = kNumOps - 1; i >= 0; --i)
            {
                if (! opActive[i]) { o[i] = 0; continue; }
                const OpSnap& op = s.op[(size_t) i];

                float pm = 0;
                for (int m = 0; m < s.modCount[i]; ++m)
                {
                    const int j = s.modList[i][m];
                    pm += s.modGain[j][i] * o[j];
                }
                const double ph = (double) (pm * fmAmt + fbAmt[i] * (fb1[u][i] + fb2[u][i]) * 0.5f);

                float v;
                if (useGrain[i])
                {
                    v = renderGrains (u, i, op, ph, inc[i] * uniMul[u], scan[i], s.hermite);
                }
                else if (useSample[i])
                {
                    v = readSample (u, i, op, ph);
                    advanceSample (u, i, op, inc[i] * uniMul[u]);
                }
                else
                {
                    v = Wavetable::read (phase[u][i] + ph, cur[i]);
                    double& p = phase[u][i];
                    p += inc[i] * uniMul[u];
                    if (p >= 1.0) p -= std::floor (p);
                }
                fb2[u][i] = fb1[u][i];
                fb1[u][i] = v;
                const float ov = v * g[i];
                o[i] = ov;
                if (s.isCarrier[i]) { sumL += ov * gL[u][i]; sumR += ov * gR[u][i]; }
            }
        }

        if (filterOn)
        {
            if (s.drive > 0.001f)
            {
                sumL = softClip (sumL * driveGain) * driveComp * 1.5f;
                sumR = softClip (sumR * driveGain) * driveComp * 1.5f;
            }
            sumL = filter.process (sumL, s.filterType, 0);
            sumR = filter.process (sumR, s.filterType, 1);
        }

        float vol = volume;
        if (stealLeft > 0)
        {
            vol *= (float) stealLeft / (float) kStealLen;
            if (--stealLeft == 0)
            {
                // дорендерили затухание — оставшиеся сэмплы чанка пропускаем, новая нота стартует со следующего
                L[k] += sumL * vol;
                R[k] += sumR * vol;
                for (int i = 0; i < kNumOps; ++i) prevAmp[i] = amp[i];
                const int pn = pendingNote;
                const float pv = pendingVel, pg = pendingGlide;
                active = false;
                for (auto& e : env) e.reset();
                modEnv.reset();
                start (pn, pv, pg, s, true);
                return;
            }
        }

        L[k] += sumL * vol;
        R[k] += sumR * vol;
    }

    for (int i = 0; i < kNumOps; ++i) prevAmp[i] = amp[i];
    for (int k = 0; k < n; ++k) modEnv.next (s.modEnv);

    // ------------------------------------------------ конец голоса
    bool anyCarrierAlive = false, anyAlive = false, anyCarrier = false;
    for (int i = 0; i < kNumOps; ++i)
    {
        if (! s.op[(size_t) i].on) continue;
        const bool alive = ! env[(size_t) i].isIdle();
        anyAlive |= alive;
        if (s.isCarrier[i]) { anyCarrier = true; anyCarrierAlive |= alive; }
    }
    if ((anyCarrier && ! anyCarrierAlive) || (! anyCarrier && ! anyAlive))
    {
        active = false;
        releasing = false;
        for (auto& e : env) e.reset();
        modEnv.reset();
    }
}

// ===========================================================================
// Synth
void Synth::prepare (double sr, int maxBlock)
{
    sampleRate = sr;
    for (auto& v : voices) v.prepare (sr);
    for (auto& l : lfo) l.reset (sr);
    // запас под 4x оверсемплинг
    for (auto& lv : lfoValues) lv.assign ((size_t) (maxBlock * 4 / kSubBlock + 2), 0.0f);
    monoStack.reserve (128);
}

void Synth::setSampleRate (double sr)
{
    sampleRate = sr;
    for (auto& v : voices) { v.setSampleRate (sr); v.kill(); }
    for (auto& l : lfo) l.setSampleRate (sr);
}

int Synth::getLowestHeldNote() const noexcept
{
    int lowest = -1;
    for (auto& v : voices)
        if (v.isActive() && ! v.isReleasing() && (lowest < 0 || v.getNote() < lowest)) lowest = v.getNote();
    return lowest;
}

void Synth::reset()
{
    for (auto& v : voices) v.kill();
    monoStack.clear();
    sustainPedal = false;
    lastPitch = -1.0f;
}

int Synth::getActiveVoiceCount() const noexcept
{
    int c = 0;
    for (auto& v : voices) c += v.isActive() ? 1 : 0;
    return c;
}

void Synth::allNotesOff()
{
    for (auto& v : voices) v.release();
    monoStack.clear();
}

void Synth::noteOn (int note, float vel, const Snapshot& s)
{
    const float glideFrom = lastPitch;
    lastPitch = (float) note;

    if (s.polyMode != PolyPoly)
    {
        Voice& v = voices[0];
        const bool anyHeld = ! monoStack.empty();
        monoStack.erase (std::remove (monoStack.begin(), monoStack.end(), note), monoStack.end());
        monoStack.push_back (note);

        if (v.isActive() && anyHeld && s.polyMode == PolyLegato)
            v.legatoTo (note, s);
        else
            v.start (note, vel, v.isActive() ? glideFrom : (s.glideSec > 0 ? glideFrom : -1.0f), s, false);
        v.age = ++ageCounter;
        return;
    }

    // та же нота уже звучит — перезапускаем этот голос
    for (auto& v : voices)
        if (v.isActive() && v.getNote() == note)
        {
            v.start (note, vel, glideFrom, s, false);
            v.age = ++ageCounter;
            return;
        }

    for (auto& v : voices)
        if (! v.isActive())
        {
            v.start (note, vel, glideFrom, s, true);
            v.age = ++ageCounter;
            return;
        }

    // кража: сначала самый тихий из отпущенных, иначе самый старый
    Voice* victim = nullptr;
    float quietest = 1e9f;
    for (auto& v : voices)
        if (v.isReleasing() && v.getLoudness() < quietest) { quietest = v.getLoudness(); victim = &v; }
    if (victim == nullptr)
        for (auto& v : voices)
            if (victim == nullptr || v.age < victim->age) victim = &v;

    victim->beginSteal (note, vel, glideFrom);
    victim->age = ++ageCounter;
}

void Synth::noteOff (int note, const Snapshot& s)
{
    if (s.drone) return;   // Drone: ноты «залипают» до выключения режима
    if (s.polyMode != PolyPoly)
    {
        monoStack.erase (std::remove (monoStack.begin(), monoStack.end(), note), monoStack.end());
        Voice& v = voices[0];
        if (! v.isActive() || v.getNote() != note) return;
        if (! monoStack.empty())
        {
            v.legatoTo (monoStack.back(), s);   // возвращаемся к предыдущей зажатой клавише
            lastPitch = (float) monoStack.back();
        }
        else if (sustainPedal) v.sustained = true, v.held = false;
        else v.release();
        return;
    }

    for (auto& v : voices)
        if (v.isActive() && v.held && v.getNote() == note)
        {
            if (sustainPedal) { v.held = false; v.sustained = true; }
            else v.release();
        }
}

void Synth::handleMidi (const juce::MidiMessage& m, const Snapshot& s)
{
    if (m.isNoteOn())
    {
        keyDown[m.getNoteNumber() & 127] = true;
        noteOn (m.getNoteNumber(), m.getFloatVelocity(), s);
    }
    else if (m.isNoteOff())
    {
        keyDown[m.getNoteNumber() & 127] = false;
        noteOff (m.getNoteNumber(), s);
    }
    else if (m.isPitchWheel())
        bendNorm = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
    else if (m.isChannelPressure())
        aftertouch = (float) m.getChannelPressureValue() / 127.0f;
    else if (m.isAftertouch())
        aftertouch = juce::jmax (aftertouch, (float) m.getAfterTouchValue() / 127.0f);
    else if (m.isController())
    {
        const int cc = m.getControllerNumber();
        if (cc == 1) modWheel = (float) m.getControllerValue() / 127.0f;
        else if (cc == 64)
        {
            sustainPedal = m.getControllerValue() >= 64;
            if (! sustainPedal)
                for (auto& v : voices)
                    if (v.isActive() && v.sustained && ! v.held) v.release();
        }
        else if (cc == 123 || cc == 120)
            allNotesOff();
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
        allNotesOff();
}

void Synth::render (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, const Snapshot& snap, const BlockInfo& info)
{
    const int numSamples = buffer.getNumSamples();
    if (snap.polyMode != lastPolyMode)
    {
        // смена режима — гасим всё, чтобы не осталось «висящих» голосов
        for (auto& v : voices) v.release();
        monoStack.clear();
        lastPolyMode = snap.polyMode;
    }

    if (lastDrone && ! snap.drone)
    {
        // Drone выключили — отпускаем всё, что не зажато физически
        if (snap.polyMode != PolyPoly)
        {
            monoStack.erase (std::remove_if (monoStack.begin(), monoStack.end(), [this] (int n) { return ! keyDown[n & 127]; }),
                             monoStack.end());
            if (monoStack.empty()) voices[0].release();
        }
        else
            for (auto& v : voices)
                if (v.isActive() && ! keyDown[v.getNote() & 127] && ! sustainPedal) v.release();
    }
    lastDrone = snap.drone;

    for (int l = 0; l < 2; ++l)
        lfo[(size_t) l].process (snap.lfo[l], info, numSamples, lfoValues[l]);

    buffer.clear();
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : L;

    BlockContext ctx;
    ctx.snap = &snap;
    ctx.lfo[0] = lfoValues[0].data();
    ctx.lfo[1] = lfoValues[1].data();
    ctx.sampleRate = sampleRate;

    auto renderRange = [&] (int from, int to)
    {
        if (to <= from) return;
        ctx.modWheel = modWheel;
        ctx.aftertouch = aftertouch;
        ctx.bendSemis = bendNorm * snap.bendRange;
        for (auto& v : voices)
            if (v.isActive())
                v.render (L + from, R + from, from, to - from, ctx);
    };

    int pos = 0;
    for (const auto meta : midi)
    {
        const int t = juce::jlimit (0, numSamples, meta.samplePosition);
        renderRange (pos, t);
        pos = t;
        handleMidi (meta.getMessage(), snap);
    }
    renderRange (pos, numSamples);

    if (R == L)   // моно-выход: среднее
        juce::FloatVectorOperations::multiply (L, 0.5f, numSamples);
}
} // namespace loom
