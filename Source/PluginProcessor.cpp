#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Algorithms.h"

namespace mist
{
MistProcessor::MistProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MIST", createParameterLayout()),
      presets (*this)
{
    auto P = [this] (const juce::String& id)
    {
        auto* p = apvts.getRawParameterValue (id);
        jassert (p != nullptr);
        return p;
    };

    for (int i = 0; i < kNumOps; ++i)
    {
        auto& o = opP[(size_t) i];
        o.on = P (opId (i, "on"));           o.mode = P (opId (i, "mode"));
        o.coarse = P (opId (i, "coarse"));   o.fine = P (opId (i, "fine"));
        o.detune = P (opId (i, "detune"));   o.fixed = P (opId (i, "fixed"));
        o.fixedHz = P (opId (i, "fixedHz")); o.level = P (opId (i, "level"));
        o.velSens = P (opId (i, "velSens")); o.feedback = P (opId (i, "feedback"));
        o.scan = P (opId (i, "scan"));       o.pan = P (opId (i, "pan"));
        o.out = P (opId (i, "out"));
        o.delay = P (opId (i, "delay"));     o.attack = P (opId (i, "attack"));
        o.hold = P (opId (i, "hold"));       o.decay = P (opId (i, "decay"));
        o.sustain = P (opId (i, "sustain")); o.release = P (opId (i, "release"));
        o.start = P (opId (i, "start"));     o.loopStart = P (opId (i, "loopStart"));
        o.loopEnd = P (opId (i, "loopEnd")); o.xfade = P (opId (i, "xfade"));
        o.loopMode = P (opId (i, "loopMode")); o.reverse = P (opId (i, "reverse"));
        o.root = P (opId (i, "root"));       o.keytrack = P (opId (i, "keytrack"));
        o.grainSize = P (opId (i, "grainSize")); o.grainDensity = P (opId (i, "grainDensity"));
        o.grainSpray = P (opId (i, "grainSpray"));
        for (int d = 0; d < kNumOps; ++d)
            if (d != i) mat[i][d] = P (matId (i, d));
    }

    pAlgorithm = P ("algorithm"); pFm = P ("fmAmount"); pMaster = P ("master");
    pPolyMode = P ("polyMode"); pGlide = P ("glide"); pBend = P ("bendRange");
    pUniVoices = P ("uniVoices"); pUniDetune = P ("uniDetune"); pUniSpread = P ("uniSpread");
    pDrone = P ("drone"); pInterp = P ("interp"); pOversampling = P ("oversampling");
    pFilterOn = P ("filterOn"); pFilterType = P ("filterType"); pCutoff = P ("cutoff"); pReso = P ("reso");
    pFilterEnv = P ("filterEnv"); pFilterKey = P ("filterKey"); pDrive = P ("drive");
    pMA = P ("menv_attack"); pMD = P ("menv_decay"); pMS = P ("menv_sustain"); pMR = P ("menv_release");
    for (int l = 0; l < 2; ++l)
    {
        pLfo[l][0] = P (lfoId (l, "shape")); pLfo[l][1] = P (lfoId (l, "rate"));
        pLfo[l][2] = P (lfoId (l, "sync"));  pLfo[l][3] = P (lfoId (l, "div"));
    }
    for (int s = 0; s < kNumModSlots; ++s)
    {
        pSlot[s][0] = P (modId (s, "src")); pSlot[s][1] = P (modId (s, "dst")); pSlot[s][2] = P (modId (s, "amt"));
    }

    (void) Wavetable::getBasic();   // построить таблицу заранее, не в аудиопотоке
}

MistProcessor::~MistProcessor()
{
    alive->store (false);
    loaderPool.removeAllJobs (true, 5000);
}

bool MistProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void MistProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    hostSampleRate = sampleRate;
    maxBlock = juce::jmax (samplesPerBlock, 4096);
    using OS = juce::dsp::Oversampling<float>;
    for (int k = 1; k <= 2; ++k)
    {
        oversamplers[(size_t) k] = std::make_unique<OS> (2, (size_t) k, OS::filterHalfBandPolyphaseIIR, true, false);
        oversamplers[(size_t) k]->initProcessing ((size_t) maxBlock);
    }
    scaledMidi.ensureSize (4096);
    synth.prepare (sampleRate, maxBlock);
    osIndex = -1;   // применится в первом processBlock
}

// ---------------------------------------------------------------------------
void MistProcessor::buildSnapshot()
{
    const double sr = currentSampleRate;   // эффективная частота (с учётом оверсемплинга)
    auto b = [] (std::atomic<float>* p) { return p->load() > 0.5f; };
    auto f = [] (std::atomic<float>* p) { return p->load(); };
    auto iv = [] (std::atomic<float>* p) { return (int) std::lround (p->load()); };

    for (int i = 0; i < kNumOps; ++i)
    {
        auto& o = snap.op[(size_t) i];
        auto& p = opP[(size_t) i];
        o.on = b (p.on);
        o.mode = iv (p.mode);
        o.ratio = coarseToRatio (iv (p.coarse)) * (1.0f + f (p.fine));
        o.detuneMul = std::exp2 (f (p.detune) / 1200.0f);
        o.fixed = b (p.fixed);
        o.fixedHz = f (p.fixedHz);
        o.level = f (p.level);
        o.velSens = f (p.velSens);
        o.feedback = f (p.feedback);
        o.scan = f (p.scan);
        o.pan = f (p.pan);
        o.env.set (sr, f (p.delay), f (p.attack), f (p.hold), f (p.decay), f (p.sustain), f (p.release));
        o.start = f (p.start);
        o.loopStart = f (p.loopStart);
        o.loopEnd = juce::jmax (o.loopStart, f (p.loopEnd));
        o.xfade = f (p.xfade);
        o.loopMode = iv (p.loopMode);
        o.reverse = b (p.reverse);
        o.root = iv (p.root);
        o.keytrack = b (p.keytrack);
        o.grainSize = f (p.grainSize);
        o.grainDensity = f (p.grainDensity);
        o.grainSpray = f (p.grainSpray);
        // asset/table выставляются в processBlock из удерживаемых на блок ссылок
    }

    const int algoChoice = iv (pAlgorithm);
    if (algoChoice == 0)
    {
        for (int s = 0; s < kNumOps; ++s)
        {
            for (int d = 0; d < kNumOps; ++d)
                snap.modGain[s][d] = s == d ? 0.0f : mat[s][d]->load();
            snap.outGain[s] = f (opP[(size_t) s].out);
            snap.isCarrier[s] = snap.outGain[s] > 0.0f;
        }
        snap.carrierNorm = 1.0f;
    }
    else
    {
        const auto& a = getAlgorithm (algoChoice - 1);
        for (int s = 0; s < kNumOps; ++s)
        {
            for (int d = 0; d < kNumOps; ++d)
                snap.modGain[s][d] = a.modulates[(size_t) s][(size_t) d] ? 1.0f : 0.0f;
            snap.outGain[s] = a.carrier[(size_t) s] ? 1.0f : 0.0f;
            snap.isCarrier[s] = a.carrier[(size_t) s];
        }
        snap.carrierNorm = 1.0f / std::sqrt ((float) juce::jmax (1, a.numCarriers));
    }
    for (int d = 0; d < kNumOps; ++d)
    {
        snap.modCount[d] = 0;
        for (int s = 0; s < kNumOps; ++s)
            if (snap.modGain[s][d] > 0.0f && snap.op[(size_t) s].on)
                snap.modList[d][snap.modCount[d]++] = s;
    }

    snap.fmAmount = f (pFm);
    snap.masterGain = juce::Decibels::decibelsToGain (f (pMaster), -48.0f);
    snap.polyMode = iv (pPolyMode);
    snap.glideSec = f (pGlide);
    snap.bendRange = f (pBend);
    snap.uniVoices = iv (pUniVoices);
    snap.uniDetune = f (pUniDetune);
    snap.uniSpread = f (pUniSpread);
    snap.drone = b (pDrone);
    snap.hermite = iv (pInterp) == InterpHermite;

    snap.filterOn = b (pFilterOn);
    snap.filterType = iv (pFilterType);
    snap.cutoff = f (pCutoff);
    snap.reso = f (pReso);
    snap.filterEnv = f (pFilterEnv);
    snap.filterKey = f (pFilterKey);
    snap.drive = f (pDrive);
    snap.modEnv.set (sr, 0.0f, f (pMA), 0.0f, f (pMD), f (pMS), f (pMR));

    snap.anyModulation = false;
    for (int s = 0; s < kNumModSlots; ++s)
    {
        auto& sl = snap.slots[(size_t) s];
        sl.src = iv (pSlot[s][0]);
        sl.dst = iv (pSlot[s][1]);
        sl.amt = f (pSlot[s][2]);
        snap.anyModulation |= (sl.src > 0 && sl.dst > 0 && sl.amt != 0.0f);
    }

    for (int l = 0; l < 2; ++l)
    {
        snap.lfo[l].shape = iv (pLfo[l][0]);
        snap.lfo[l].rateHz = f (pLfo[l][1]);
        snap.lfo[l].sync = b (pLfo[l][2]);
        snap.lfo[l].div = iv (pLfo[l][3]);
    }
}

void MistProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const auto t0 = juce::Time::getHighResolutionTicks();
    const int numSamples = buffer.getNumSamples();
    for (int c = 2; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, numSamples);

    // оверсемплинг: 0 = 1x, 1 = 2x, 2 = 4x
    const int wantOs = juce::jlimit (0, 2, (int) std::lround (pOversampling->load()));
    if (wantOs != osIndex)
    {
        osIndex = wantOs;
        currentSampleRate = hostSampleRate * (1 << osIndex);
        synth.setSampleRate (currentSampleRate);
        if (osIndex > 0) oversamplers[(size_t) osIndex]->reset();
        setLatencySamples (osIndex > 0 ? (int) std::lround (oversamplers[(size_t) osIndex]->getLatencyInSamples()) : 0);
    }

    // держим ссылки на ассеты этого блока
    std::array<std::shared_ptr<const SampleAsset>, kNumOps> hold;
    for (int i = 0; i < kNumOps; ++i) hold[(size_t) i] = std::atomic_load (&samples[(size_t) i]);

    buildSnapshot();
    for (int i = 0; i < kNumOps; ++i)
    {
        snap.op[(size_t) i].asset = hold[(size_t) i].get();
        snap.op[(size_t) i].table = hold[(size_t) i] ? &hold[(size_t) i]->table : &Wavetable::getBasic();
    }

    blockInfo = {};
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) blockInfo.bpm = *bpm;
            if (auto ppq = pos->getPpqPosition()) { blockInfo.ppq = *ppq; blockInfo.hasPpq = true; }
            blockInfo.playing = pos->getIsPlaying();
        }

    if (osIndex == 0 || buffer.getNumChannels() < 2)
    {
        synth.render (buffer, midi, snap, blockInfo);
    }
    else
    {
        auto& os = *oversamplers[(size_t) osIndex];
        const int factor = 1 << osIndex;
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), 2, (size_t) numSamples);
        block.clear();
        auto up = os.processSamplesUp (block);   // получаем буфер повышенной частоты (вход — тишина)
        float* chans[2] = { up.getChannelPointer (0), up.getChannelPointer (1) };
        juce::AudioBuffer<float> upBuf (chans, 2, (int) up.getNumSamples());

        scaledMidi.clear();
        for (const auto m : midi)
            scaledMidi.addEvent (m.getMessage(), m.samplePosition * factor);
        synth.render (upBuf, scaledMidi, snap, blockInfo);
        os.processSamplesDown (block);
    }

    // защитный мягкий лимитер: прозрачен до -2 dBFS, выше — плавно ограничивает до 0 dBFS
    float peak = 0.0f;
    for (int c = 0; c < juce::jmin (2, buffer.getNumChannels()); ++c)
    {
        float* d = buffer.getWritePointer (c);
        for (int i = 0; i < numSamples; ++i)
        {
            const float x = d[i], ax = std::abs (x);
            if (ax > 0.8f)
                d[i] = std::copysign (0.8f + 0.2f * std::tanh ((ax - 0.8f) * 5.0f), x);
            peak = juce::jmax (peak, std::abs (d[i]));
        }
    }

    // данные для визуализации (стерео)
    {
        const float* l = buffer.getReadPointer (0);
        const float* r = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : l;
        const auto w = scopeFifo.write (juce::jmin (numSamples, scopeFifo.getFreeSpace()));
        int k = 0;
        for (int i = 0; i < w.blockSize1; ++i, ++k) { scopeL[(size_t) (w.startIndex1 + i)] = l[k]; scopeR[(size_t) (w.startIndex1 + i)] = r[k]; }
        for (int i = 0; i < w.blockSize2; ++i, ++k) { scopeL[(size_t) (w.startIndex2 + i)] = l[k]; scopeR[(size_t) (w.startIndex2 + i)] = r[k]; }
    }
    lfoDisplay[0].store (synth.getLfoValue (0));
    lfoDisplay[1].store (synth.getLfoValue (1));
    activeVoices.store (synth.getActiveVoiceCount());
    lowestNote.store (synth.getLowestHeldNote());
    outPeak.store (juce::jmax (peak, outPeak.load() * 0.92f));

    const double elapsed = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
    const double load = elapsed / juce::jmax (1.0e-6, numSamples / hostSampleRate);
    cpuLoad.store ((float) (cpuLoad.load() * 0.9 + load * 0.1));
    // hold уничтожается здесь; если это последняя ссылка — её держит garbage (см. setSample), освобождения в аудиопотоке нет
}

// ---------------------------------------------------------------------------
std::shared_ptr<const SampleAsset> MistProcessor::getSample (int op) const
{
    return std::atomic_load (&samples[(size_t) op]);
}

void MistProcessor::setSample (int op, std::shared_ptr<const SampleAsset> asset)
{
    auto old = std::atomic_exchange (&samples[(size_t) op], std::move (asset));
    {
        std::lock_guard<std::mutex> lock (garbageMutex);
        if (old) garbage.push_back (std::move (old));
    }
    ++sampleVersion;
    collectGarbage();
}

void MistProcessor::collectGarbage()
{
    std::lock_guard<std::mutex> lock (garbageMutex);
    // освобождаем только то, что никто, кроме нас, не держит (аудиопоток отпускает ссылку в конце блока)
    garbage.erase (std::remove_if (garbage.begin(), garbage.end(),
                                   [] (const std::shared_ptr<const SampleAsset>& p) { return p.use_count() <= 1; }),
                   garbage.end());
}

juce::String MistProcessor::getLastError() const
{
    std::lock_guard<std::mutex> lock (errorMutex);
    return lastError;
}

void MistProcessor::loadSampleAsync (int op, const juce::File& file)
{
    auto aliveFlag = alive;
    loaderPool.addJob ([this, op, file, aliveFlag]
    {
        juce::String err;
        auto asset = SampleAsset::loadFromFile (file, err);
        if (! aliveFlag->load()) return;

        if (asset == nullptr)
        {
            std::lock_guard<std::mutex> lock (errorMutex);
            lastError = err;
            ++sampleVersion;
            return;
        }

        const int root = asset->detectedMidi > 0 ? juce::jlimit (0, 127, (int) std::lround (asset->detectedMidi)) : 60;
        setSample (op, std::move (asset));
        {
            std::lock_guard<std::mutex> lock (errorMutex);
            lastError = {};
        }

        juce::MessageManager::callAsync ([this, op, root, aliveFlag]
        {
            if (! aliveFlag->load()) return;
            if (auto* p = apvts.getParameter (opId (op, "root")))
                p->setValueNotifyingHost (p->convertTo0to1 ((float) root));
            collectGarbage();
        });
    });
}

void MistProcessor::clearSample (int op)
{
    setSample (op, nullptr);
}

// ---------------------------------------------------------------------------
std::unique_ptr<juce::XmlElement> MistProcessor::createStateXml (bool includeSamples)
{
    auto state = apvts.copyState();
    auto xml = state.createXml();
    xml->setAttribute ("mistVersion", 1);

    auto* samplesXml = xml->createNewChildElement ("SAMPLES");
    if (includeSamples)
        for (int i = 0; i < kNumOps; ++i)
            if (auto a = getSample (i))
            {
                auto* s = samplesXml->createNewChildElement ("SAMPLE");
                s->setAttribute ("op", i);
                s->setAttribute ("name", a->name);
                s->setAttribute ("data", a->encoded.toBase64Encoding());
            }
    return xml;
}

void MistProcessor::restoreStateXml (const juce::XmlElement& xml)
{
    if (! xml.hasTagName (apvts.state.getType())) return;

    auto copy = std::make_unique<juce::XmlElement> (xml);
    // семплы храним отдельно от дерева параметров
    std::array<std::shared_ptr<SampleAsset>, kNumOps> loaded {};
    if (auto* s = copy->getChildByName ("SAMPLES"))
    {
        for (auto* e : s->getChildIterator())
        {
            const int op = e->getIntAttribute ("op", -1);
            if (op < 0 || op >= kNumOps) continue;
            juce::MemoryBlock mb;
            if (! mb.fromBase64Encoding (e->getStringAttribute ("data"))) continue;
            juce::String err;
            loaded[(size_t) op] = SampleAsset::loadFromEncoded (mb, e->getStringAttribute ("name"), err);
        }
        copy->removeChildElement (s, true);
    }

    apvts.replaceState (juce::ValueTree::fromXml (*copy));
    for (int i = 0; i < kNumOps; ++i)
        setSample (i, loaded[(size_t) i]);
}

void MistProcessor::resetToDefaults()
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());
    for (int i = 0; i < kNumOps; ++i)
        setSample (i, nullptr);
}

void MistProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto xml = createStateXml (true);
    xml->setAttribute ("presetName", presets.getCurrentName());
    copyXmlToBinary (*xml, destData);
}

void MistProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        restoreStateXml (*xml);
        presets.setCurrentName (xml->getStringAttribute ("presetName", "Init"));
    }
}

juce::AudioProcessorEditor* MistProcessor::createEditor()
{
    return new MistEditor (*this);
}
} // namespace mist

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new mist::MistProcessor();
}
