#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Params.h"
#include "DSP/Synth.h"
#include "DSP/SampleAsset.h"
#include "PresetManager.h"
#include <atomic>
#include <mutex>

namespace mist
{
class MistProcessor : public juce::AudioProcessor
{
public:
    MistProcessor();
    ~MistProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Mist"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- состояние целиком (параметры + семплы) — используется и пресетами
    std::unique_ptr<juce::XmlElement> createStateXml (bool includeSamples = true);
    void restoreStateXml (const juce::XmlElement& xml);
    void resetToDefaults();          // всё по умолчанию: параметры, семплы, имена слотов (Init patch)
    void resetForTemplate();         // для шаблонов: параметры по умолчанию, но семплы, их настройки,
                                     // имена слотов и Transpose остаются как есть

    // ---- имена слотов (двойной клик по «Op N»). Пустое имя = «Op N». Хранятся в состоянии и пресетах.
    static constexpr int kMaxOpNameLength = 16;
    juce::String getOpName (int op) const;          // то, что показывать: своё имя или «Op N»
    juce::String getCustomOpName (int op) const;    // только своё имя (может быть пустым)
    void setOpName (int op, const juce::String& name);

    // ---- семплы
    void loadSampleAsync (int op, const juce::File& file);
    void clearSample (int op);
    std::shared_ptr<const SampleAsset> getSample (int op) const;
    int getSampleVersion() const noexcept { return sampleVersion.load(); }
    juce::String getLastError() const;

    // ---- для UI
    void playJumpSound() { jumpSoundPending.store (true); }
    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;
    static constexpr int kScopeSize = 16384;
    juce::AbstractFifo scopeFifo { kScopeSize };
    std::vector<float> scopeL = std::vector<float> (kScopeSize, 0.0f), scopeR = std::vector<float> (kScopeSize, 0.0f);
    std::atomic<float> outPeak { 0.0f }, cpuLoad { 0.0f };
    std::atomic<int> lowestNote { -1 };
    std::atomic<float> lfoDisplay[2] {};
    std::atomic<int> activeVoices { 0 };

private:
    void setSample (int op, std::shared_ptr<const SampleAsset> asset);
    void collectGarbage();
    void buildSnapshot();
    void renderJumpSound (juce::AudioBuffer<float>& buffer);

    // короткий «прыжок»: пила со скольжением вверх, подмешивается к выходу
    std::atomic<bool> jumpSoundPending { false };
    int jumpSamplesLeft = 0, jumpLength = 0;
    double jumpPhase = 0.0;

    Synth synth;
    Snapshot snap;
    BlockInfo blockInfo;

    std::array<std::shared_ptr<const SampleAsset>, kNumOps> samples;   // доступ через std::atomic_load/store
    std::vector<std::shared_ptr<const SampleAsset>> garbage;
    std::mutex garbageMutex;
    std::atomic<int> sampleVersion { 0 };
    juce::String lastError;
    mutable std::mutex errorMutex;
    juce::ThreadPool loaderPool { 1 };
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);

    // кэш указателей на параметры
    struct OpParamPtrs
    {
        std::atomic<float> *on, *mode, *coarse, *fine, *detune, *fixed, *fixedHz, *level, *velSens, *feedback,
                           *scan, *pan, *out, *delay, *attack, *hold, *decay, *sustain, *release,
                           *aCurve, *dCurve, *rCurve,
                           *start, *loopStart, *loopEnd, *xfade, *loopMode, *reverse, *root, *keytrack,
                           *grainSize, *grainDensity, *grainSpray;
    };
    std::array<OpParamPtrs, kNumOps> opP {};
    std::atomic<float>* mat[kNumOps][kNumOps] {};
    std::atomic<float> *pAlgorithm, *pFm, *pMaster, *pPolyMode, *pGlide, *pBend, *pUniVoices, *pUniDetune, *pUniSpread,
                       *pFilterOn, *pFilterType, *pCutoff, *pReso, *pFilterEnv, *pFilterKey, *pDrive,
                       *pMA, *pMD, *pMS, *pMR, *pMAc, *pMDc, *pMRc, *pTranspose, *pHold;
    std::atomic<float>* pLfo[2][4] {};
    std::atomic<float>* pSlot[kNumModSlots][3] {};

    double currentSampleRate = 44100.0, hostSampleRate = 44100.0;
    int maxBlock = 4096, osIndex = -1;
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 3> oversamplers;
    juce::MidiBuffer scaledMidi;
    std::atomic<float> *pDrone = nullptr, *pInterp = nullptr, *pOversampling = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MistProcessor)
};
} // namespace mist
