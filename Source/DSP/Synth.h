#pragma once
#include "../Params.h"
#include "Primitives.h"
#include "SampleAsset.h"
#include <array>
#include <vector>

namespace mist
{
// ---------------------------------------------------------------------------
// Снимок параметров на блок (читается из APVTS в processBlock)
struct OpSnap
{
    bool on = true;
    int mode = OpModeWave;
    float ratio = 1.0f;
    float detuneMul = 1.0f;
    bool fixed = false;
    float fixedHz = 110.0f;
    float level = 0.0f;
    float velSens = 0.5f;
    float feedback = 0.0f;
    float scan = 0.0f;
    float pan = 0.0f;
    EnvCoefs env;
    float start = 0, loopStart = 0, loopEnd = 1, xfade = 0.1f;
    int loopMode = LoopForward;
    bool reverse = false;
    int root = 60;
    bool keytrack = true;
    float grainSize = 0.12f, grainDensity = 3.0f, grainSpray = 0.1f;
    const SampleAsset* asset = nullptr;   // nullptr = нет семпла
    const Wavetable* table = nullptr;     // всегда валиден
};

struct ModSlot { int src = 0, dst = 0; float amt = 0; };

struct Snapshot
{
    std::array<OpSnap, kNumOps> op;
    float modGain[kNumOps][kNumOps] {};   // [src][dst]
    float outGain[kNumOps] {};
    int modList[kNumOps][kNumOps] {};     // для dst: список src с ненулевым gain
    int modCount[kNumOps] {};
    bool isCarrier[kNumOps] {};
    float carrierNorm = 1.0f;

    float fmAmount = 1.0f;
    float masterGain = 0.5f;
    int polyMode = PolyPoly;
    float glideSec = 0.0f;
    float bendRange = 2.0f;
    int uniVoices = 1;
    float uniDetune = 0.0f;
    float uniSpread = 0.0f;

    bool filterOn = false;
    int filterType = FiltLP24;
    float cutoff = 8000.0f, reso = 0.1f, filterEnv = 0.0f, filterKey = 0.0f, drive = 0.0f;
    EnvCoefs modEnv;

    std::array<ModSlot, kNumModSlots> slots;
    bool anyModulation = false;
    bool drone = false;
    bool hermite = true;

    struct LfoSettings { int shape = 0; float rateHz = 1.0f; bool sync = false; int div = 3; } lfo[2];
};

struct BlockInfo
{
    double bpm = 120.0;
    double ppq = 0.0;
    bool playing = false;
    bool hasPpq = false;
};

// ---------------------------------------------------------------------------
class Lfo
{
public:
    void reset (double sr) { sampleRate = sr; phase = 0; held = 0; prevHeld = 0; }
    void setSampleRate (double sr) { sampleRate = sr; }
    // заполняет out[k] значением на начале каждого подблока
    void process (const Snapshot::LfoSettings& s, const BlockInfo& info, int numSamples, std::vector<float>& out);
    float getLastValue() const noexcept { return lastValue; }

private:
    float shapeValue (int shape, double ph) noexcept;
    double sampleRate = 44100.0, phase = 0.0;
    float held = 0.0f, prevHeld = 0.0f, lastValue = 0.0f;
    juce::Random rng { 1234 };
};

struct BlockContext
{
    const Snapshot* snap = nullptr;
    const float* lfo[2] { nullptr, nullptr };
    float modWheel = 0.0f;
    float aftertouch = 0.0f;
    float bendSemis = 0.0f;
    double sampleRate = 44100.0;
};

// ---------------------------------------------------------------------------
class Voice
{
public:
    void prepare (double sr);
    void setSampleRate (double sr) { sampleRate = sr; }
    void start (int note, float velocity, float glideFromPitch, const Snapshot& s, bool resetPhases);
    void legatoTo (int note, const Snapshot& s);
    void release();
    void kill();
    void beginSteal (int newNote, float newVel, float glideFrom);

    // рендер в L/R начиная с absStart (позиция в блоке, нужна для индекса LFO)
    void render (float* L, float* R, int absStart, int numSamples, const BlockContext& ctx);

    bool isActive() const noexcept { return active; }
    bool isReleasing() const noexcept { return releasing; }
    int getNote() const noexcept { return note; }
    float getLoudness() const noexcept;

    bool held = false;        // клавиша нажата
    bool sustained = false;   // держится педалью
    juce::uint32 age = 0;

private:
    void renderChunk (float* L, float* R, int n, int lfoIndex, const BlockContext& ctx);
    inline float readSample (int u, int i, const OpSnap& o, double pmCycles) noexcept;
    inline void advanceSample (int u, int i, const OpSnap& o, double inc) noexcept;
    void initSampleState (int u, int i, const OpSnap& o) noexcept;
    inline float renderGrains (int u, int i, const OpSnap& o, double pmCycles, double inc, float scanPos, bool hermite) noexcept;

    double sampleRate = 44100.0;
    bool active = false, releasing = false;
    int note = 60;
    float velocity = 1.0f;
    float randomValue = 0.0f;
    float pitch = 60.0f, targetPitch = 60.0f, glideStep = 0.0f;

    std::array<Envelope, kNumOps> env;
    Envelope modEnv;
    std::array<float, kNumOps> prevAmp {};

    double phase[kMaxUnison][kNumOps] {};
    double spos[kMaxUnison][kNumOps] {};
    int sdir[kMaxUnison][kNumOps] {};
    bool sdone[kMaxUnison][kNumOps] {};
    float out[kMaxUnison][kNumOps] {};
    float fb1[kMaxUnison][kNumOps] {}, fb2[kMaxUnison][kNumOps] {};
    int lastAssetLength[kNumOps] {};

    // Grain: до kMaxGrains перекрывающихся зёрен на (унисон, оператор)
    static constexpr int kMaxGrains = 6;
    struct Grain { double pos = 0; float age = 0, len = 0; signed char dir = 1; bool on = false; };
    Grain grains[kMaxUnison][kNumOps][kMaxGrains];
    float grainTimer[kMaxUnison][kNumOps] {};

    SVF filter;

    // steal
    int stealLeft = 0;
    static constexpr int kStealLen = 96;
    int pendingNote = -1;
    float pendingVel = 0, pendingGlide = 0;

    juce::Random rng;
    bool hermiteRead = true;
};

// ---------------------------------------------------------------------------
class Synth
{
public:
    void prepare (double sr, int maxBlock);
    void setSampleRate (double sr);   // без аллокаций (смена оверсемплинга)
    int getLowestHeldNote() const noexcept;
    void reset();
    void render (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, const Snapshot& snap, const BlockInfo& info);

    float getLfoValue (int i) const noexcept { return lfo[(size_t) i].getLastValue(); }
    int getActiveVoiceCount() const noexcept;

private:
    void handleMidi (const juce::MidiMessage& m, const Snapshot& s);
    void noteOn (int note, float vel, const Snapshot& s);
    void noteOff (int note, const Snapshot& s);
    void allNotesOff();

    double sampleRate = 44100.0;
    std::array<Voice, kNumVoices> voices;
    std::array<Lfo, 2> lfo;
    std::vector<float> lfoValues[2];
    juce::uint32 ageCounter = 0;
    float lastPitch = -1.0f;
    bool sustainPedal = false;
    float modWheel = 0, aftertouch = 0, bendNorm = 0;
    std::vector<int> monoStack;
    int lastPolyMode = 0;
    bool keyDown[128] {};
    bool lastDrone = false;
};
} // namespace mist
