#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "DSP/Synth.h"
#include "DSP/SampleAsset.h"
#include "PresetManager.h"
#include <atomic>
#include <mutex>

namespace ferrite
{
class FerriteProcessor : public juce::AudioProcessor
{
public:
    FerriteProcessor();
    ~FerriteProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Ferrite"; }
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
    void resetToDefaults();

    // ---- семплы
    void loadSampleAsync (int op, const juce::File& file);
    void clearSample (int op);
    std::shared_ptr<const SampleAsset> getSample (int op) const;
    int getSampleVersion() const noexcept { return sampleVersion.load(); }
    juce::String getLastError() const;

    // ---- для UI
    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;
    juce::AbstractFifo scopeFifo { 8192 };
    std::vector<float> scopeBuffer = std::vector<float> (8192, 0.0f);
    std::atomic<float> lfoDisplay[2] {};
    std::atomic<int> activeVoices { 0 };

private:
    void setSample (int op, std::shared_ptr<const SampleAsset> asset);
    void collectGarbage();
    void buildSnapshot();

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
                           *start, *loopStart, *loopEnd, *xfade, *loopMode, *reverse, *root, *keytrack;
    };
    std::array<OpParamPtrs, kNumOps> opP {};
    std::atomic<float>* mat[kNumOps][kNumOps] {};
    std::atomic<float> *pAlgorithm, *pFm, *pMaster, *pPolyMode, *pGlide, *pBend, *pUniVoices, *pUniDetune, *pUniSpread,
                       *pFilterOn, *pFilterType, *pCutoff, *pReso, *pFilterEnv, *pFilterKey, *pDrive,
                       *pMA, *pMD, *pMS, *pMR;
    std::atomic<float>* pLfo[2][4] {};
    std::atomic<float>* pSlot[kNumModSlots][3] {};

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FerriteProcessor)
};
} // namespace ferrite
