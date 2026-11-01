// Офлайн-тест: рендерит пресеты/алгоритмы в WAV, проверяет NaN, пики и скорость, снимает скриншот UI.
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"
#include "Randomizer.h"

using namespace mist;

static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { ++failures; std::cout << "  FAIL: " << msg << "\n"; } } while (0)

static void setParam (MistProcessor& p, const juce::String& id, float v)
{
    auto* pp = p.apvts.getParameter (id);
    if (pp == nullptr) { std::cout << "  unknown param " << id << "\n"; ++failures; return; }
    pp->setValueNotifyingHost (pp->convertTo0to1 (v));
}

struct Stats { float peak = 0; double rms = 0; bool finite = true; double realtimeFactor = 0; };

static Stats render (MistProcessor& p, const std::vector<int>& notes, double holdSec, double tailSec,
                     const juce::File& out, double sr = 48000.0, int block = 512)
{
    p.prepareToPlay (sr, block);
    const int total = (int) ((holdSec + tailSec) * sr);
    const int offAt = (int) (holdSec * sr);
    juce::AudioBuffer<float> all (2, total);
    juce::AudioBuffer<float> buf (2, block);
    Stats st;
    double sum = 0;

    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = juce::jmin (block, total - pos);
        buf.setSize (2, n, false, false, true);
        juce::MidiBuffer midi;
        if (pos == 0)
            for (int nn : notes) midi.addEvent (juce::MidiMessage::noteOn (1, nn, (juce::uint8) 100), 0);
        if (offAt >= pos && offAt < pos + n)
            for (int nn : notes) midi.addEvent (juce::MidiMessage::noteOff (1, nn), offAt - pos);
        p.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c) all.copyFrom (c, pos, buf, c, 0, n);
    }
    const auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
    st.realtimeFactor = (total / sr * 1000.0) / juce::jmax (0.001, ms);

    for (int c = 0; c < 2; ++c)
    {
        const float* d = all.getReadPointer (c);
        for (int i = 0; i < total; ++i)
        {
            if (! std::isfinite (d[i])) st.finite = false;
            st.peak = juce::jmax (st.peak, std::abs (d[i]));
            sum += (double) d[i] * d[i];
        }
    }
    st.rms = std::sqrt (sum / (2.0 * total));

    if (out != juce::File())
    {
        out.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> os = std::make_unique<juce::FileOutputStream> (out);
        if (auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions{}.withSampleRate (sr).withNumChannels (2).withBitsPerSample (24)))
            w->writeFromAudioSampleBuffer (all, 0, total);
    }
    return st;
}

static void report (const juce::String& name, const Stats& s)
{
    std::cout << "  " << name.paddedRight (' ', 26) << " peak " << juce::String (s.peak, 3)
              << "  rms " << juce::String (s.rms, 4) << "  x" << juce::String (s.realtimeFactor, 1) << " realtime"
              << (s.finite ? "" : "  NaN!") << "\n";
    CHECK (s.finite, name + " produced NaN/Inf");
    CHECK (s.peak <= 1.0f, name + " peak above 0 dBFS");
}

static juce::File makeTestSample (const juce::File& dir)
{
    // 2 секунды: пила 220 Гц с вибрато, постепенно переходящая в шум
    const double sr = 44100.0;
    const int n = (int) (2.0 * sr);
    juce::AudioBuffer<float> b (1, n);
    juce::Random rng (42);
    double ph = 0;
    for (int i = 0; i < n; ++i)
    {
        const double t = i / sr;
        ph += (220.0 * (1.0 + 0.003 * std::sin (t * 2 * juce::MathConstants<double>::pi * 5))) / sr;
        ph -= std::floor (ph);
        const float saw = (float) (2 * ph - 1);
        const float mix = (float) juce::jlimit (0.0, 1.0, (t - 1.0) / 1.0);
        b.setSample (0, i, 0.7f * ((1 - mix) * saw + mix * (rng.nextFloat() * 2 - 1)));
    }
    auto f = dir.getChildFile ("test_saw_220.wav");
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os = std::make_unique<juce::FileOutputStream> (f);
    if (auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions{}.withSampleRate (sr).withNumChannels (1).withBitsPerSample (16)))
        w->writeFromAudioSampleBuffer (b, 0, n);
    return f;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir = argc > 1 ? juce::File (juce::String (argv[1])) : juce::File::getCurrentWorkingDirectory().getChildFile ("render");
    outDir.createDirectory();

    auto proc = std::make_unique<MistProcessor>();
    auto& p = *proc;

    std::cout << "== Factory presets\n";
    const auto& fp = getFactoryPresets();
    p.presets.refresh();
    for (int i = 0; i < (int) fp.size(); ++i)
    {
        p.presets.load (i);
        auto st = render (p, { 48, 55, 60, 64 }, 3.0, 3.0, outDir.getChildFile ("preset_" + juce::String (i + 1) + "_" + fp[(size_t) i].name.replace (" ", "_") + ".wav"));
        report (fp[(size_t) i].name, st);
        CHECK (st.rms > 1e-4, fp[(size_t) i].name + " is silent");
    }

    std::cout << "== All 32 algorithms, worst case (8 voices x 8 unison, FM 400%, feedback 100%)\n";
    double worstRt = 1e9;
    for (int a = 0; a <= kNumAlgorithms; ++a)
    {
        p.presets.initPatch();
        setParam (p, "algorithm", (float) a);
        setParam (p, "fmAmount", 4.0f);
        setParam (p, "uniVoices", 8);
        for (int i = 0; i < kNumOps; ++i)
        {
            setParam (p, opId (i, "level"), 1.0f);
            setParam (p, opId (i, "feedback"), 1.0f);
            setParam (p, opId (i, "coarse"), (float) (i + 1));
            setParam (p, opId (i, "out"), 1.0f);
            for (int d = 0; d < kNumOps; ++d) if (d != i) setParam (p, matId (i, d), 1.0f);
        }
        auto st = render (p, { 36, 43, 48, 55, 60, 67, 72, 79 }, 1.0, 0.5, {});
        worstRt = juce::jmin (worstRt, st.realtimeFactor);
        if (! st.finite || st.peak > 1.0f) report ("algo " + juce::String (a), st);
    }
    std::cout << "  worst realtime factor: x" << juce::String (worstRt, 1) << "\n";

    std::cout << "== Randomizer: 40 random patches must be audible, finite and below 0 dBFS\n";
    {
        juce::Random rng (7);
        int silent = 0;
        float worstPeak = 0.0f;
        for (int k = 0; k < 40; ++k)
        {
            p.presets.initPatch();
            if (k % 2) { setParam (p, "filterOn", 1.0f); setParam (p, "filterType", (float) (k % 5)); }
            randomizePatch (p, rng);
            auto st = render (p, { 48, 55, 60 }, 1.5, 0.3, {});
            if (st.rms < 1e-3) { ++silent; std::cout << "  silent patch #" << k << " algo " << p.apvts.getRawParameterValue ("algorithm")->load() << "\n"; }
            CHECK (st.finite, "random patch produced NaN");
            worstPeak = juce::jmax (worstPeak, st.peak);
        }
        std::cout << "  silent: " << silent << "/40, worst peak " << worstPeak << "\n";
        CHECK (silent == 0, "randomizer produced silent patches");
        CHECK (worstPeak <= 1.0f, "randomizer peak above 0 dBFS");
    }

    std::cout << "== Sample operator\n";
    auto sampleFile = makeTestSample (outDir);
    p.presets.initPatch();
    p.loadSampleAsync (0, sampleFile);
    for (int i = 0; i < 200 && p.getSample (0) == nullptr; ++i)
        juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    auto asset = p.getSample (0);
    CHECK (asset != nullptr, "sample did not load: " + p.getLastError());
    if (asset)
    {
        std::cout << "  loaded '" << asset->name << "' len " << asset->length << " detected midi " << asset->detectedMidi
                  << " frames " << asset->table.getNumFrames() << " flac " << (int) asset->encoded.getSize() << " bytes\n";
        CHECK (std::abs (asset->detectedMidi - 57.0f) < 0.3f, "pitch detection: expected A3 (57)");
        auto* root = p.apvts.getParameter (opId (0, "root"));
        CHECK ((int) root->convertFrom0to1 (root->getValue()) == 57, "root not set from detection");

        // wave-режим из семпла
        setParam (p, opId (0, "scan"), 0.0f);
        setParam (p, opId (1, "level"), 0.4f);
        setParam (p, opId (1, "coarse"), 2);
        report ("sample wavetable", render (p, { 57 }, 2.0, 1.0, outDir.getChildFile ("sample_wave.wav")));
        // sample-режим с PM, петлёй и унисоном
        setParam (p, opId (0, "mode"), (float) OpModeSample);
        setParam (p, opId (0, "loopStart"), 0.2f);
        setParam (p, opId (0, "loopEnd"), 0.6f);
        setParam (p, "uniVoices", 4);
        report ("sample playback PM", render (p, { 57, 64 }, 3.0, 1.0, outDir.getChildFile ("sample_play.wav")));
        setParam (p, opId (0, "reverse"), 1.0f);
        setParam (p, opId (0, "loopMode"), (float) LoopPingPong);
        report ("sample reverse pingpong", render (p, { 50 }, 3.0, 1.0, outDir.getChildFile ("sample_pingpong.wav")));
        setParam (p, opId (0, "reverse"), 0.0f);

        // Grain
        setParam (p, opId (0, "mode"), (float) OpModeGrain);
        setParam (p, opId (0, "scan"), 0.3f);
        setParam (p, opId (0, "grainSize"), 0.08f);
        setParam (p, opId (0, "grainDensity"), 4.0f);
        setParam (p, opId (0, "grainSpray"), 0.3f);
        auto gs = render (p, { 57, 64 }, 3.0, 1.0, outDir.getChildFile ("sample_grain.wav"));
        report ("grain", gs);
        CHECK (gs.rms > 1e-3, "grain mode silent");

        // интерполяция и оверсемплинг
        setParam (p, "interp", (float) InterpLinear);
        report ("grain linear interp", render (p, { 57 }, 1.0, 0.5, {}));
        setParam (p, "interp", (float) InterpHermite);
        for (int os = 0; os < 3; ++os)
        {
            setParam (p, "oversampling", (float) os);
            auto st = render (p, { 45, 57, 64 }, 1.5, 0.5, outDir.getChildFile ("os_" + juce::String (1 << os) + "x.wav"));
            report ("oversampling " + juce::String (1 << os) + "x", st);
            CHECK (st.rms > 1e-3, "oversampling " + juce::String (1 << os) + "x silent");
            CHECK (p.getLatencySamples() >= 0, "latency");
        }
        setParam (p, "oversampling", 1.0f);

        // сохранение/восстановление состояния с семплом
        juce::MemoryBlock state;
        p.getStateInformation (state);
        std::cout << "  state size " << (int) state.getSize() << " bytes\n";
        auto p2 = std::make_unique<MistProcessor>();
        p2->setStateInformation (state.getData(), (int) state.getSize());
        auto a2 = p2->getSample (0);
        CHECK (a2 != nullptr && a2->length == asset->length, "sample not restored from state");
        auto* m2 = p2->apvts.getParameter (opId (0, "mode"));
        CHECK ((int) m2->convertFrom0to1 (m2->getValue()) == OpModeGrain, "params not restored");

        // пресет-файл
        CHECK (p.presets.save ("RenderTest Preset", "Tests"), "preset save failed");
        p.presets.initPatch();
        auto f = PresetManager::getPresetFolder().getChildFile ("Tests").getChildFile ("RenderTest Preset.mist");
        CHECK (p.presets.loadFile (f), "preset load failed");
        CHECK (p.getSample (0) != nullptr, "preset did not restore sample");
        f.getParentDirectory().deleteRecursively();
    }

    // Скриншот интерфейса
    if (juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() != nullptr)
    {
        std::cout << "== Editor snapshot\n";
        p.presets.load (4);   // Breathing Swarm: Op1 — несущая, Op2 — модулятор
        if (asset)
        {
            p.loadSampleAsync (0, sampleFile);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
            p.loadSampleAsync (1, sampleFile);
        }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (500);
        setParam (p, "drone", 1.0f);
        setParam (p, opId (0, "mode"), (float) OpModeWave);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        auto* le = dynamic_cast<MistEditor*> (ed.get());
        ed->setSize (MistEditor::kBaseW, MistEditor::kBaseH);
        p.prepareToPlay (48000.0, 512);

        auto pump = [&] (int blocks, bool notes)
        {
            juce::AudioBuffer<float> buf (2, 512);
            for (int blk = 0; blk < blocks; ++blk)
            {
                juce::MidiBuffer midi;
                if (notes && blk == 0) for (int nn : { 45, 52, 57, 64 }) midi.addEvent (juce::MidiMessage::noteOn (1, nn, (juce::uint8) 100), 0);
                if (notes && blk == 1) for (int nn : { 45, 52, 57, 64 }) midi.addEvent (juce::MidiMessage::noteOff (1, nn), 0);   // Drone держит
                p.processBlock (buf, midi);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (11);
            }
        };
        auto shoot = [&] (const juce::String& name)
        {
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            auto pngFile = outDir.getChildFile (name + ".png");
            pngFile.deleteFile();
            juce::FileOutputStream fos (pngFile);
            juce::PNGImageFormat().writeImageToStream (img, fos);
            std::cout << "  wrote " << pngFile.getFullPathName() << "\n";
        };

        bool first = true;
        for (auto theme : { ThemeId::Oxygen, ThemeId::Rubber })
        {
            const juce::String tn = theme == ThemeId::Oxygen ? "oxygen" : "rubber";
            le->setThemeForTest (theme);
            le->setView (0, 2, 0);  pump (90, first); first = false; shoot ("ui_" + tn + "_scope");
            le->setView (2, 0, 1);  pump (60, false); shoot ("ui_" + tn + "_waves");
            le->setView (6, 1, 0);  pump (60, false); shoot ("ui_" + tn + "_spectrum");
            {
                // скорость: 30 кадров отрисовки главного окна в виде Spectrum
                const auto t0 = juce::Time::getMillisecondCounterHiRes();
                double paintMs = 0;
                for (int k = 0; k < 30; ++k)
                {
                    pump (1, false);
                    const auto a = juce::Time::getMillisecondCounterHiRes();
                    (void) ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
                    paintMs += juce::Time::getMillisecondCounterHiRes() - a;
                }
                juce::ignoreUnused (t0);
                std::cout << "  spectrum view paint: " << juce::String (paintMs / 30.0, 2) << " ms/frame\n";
            }
            le->showAboutForTest (true, false); pump (5, false); shoot ("about_" + tn);
            le->showAboutForTest (true, true);  pump (40, false); shoot ("game_" + tn);
            le->showAboutForTest (false, false);
        }
        le->setThemeForTest (ThemeId::Oxygen);
        CHECK (p.activeVoices.load() > 0, "drone did not hold notes");
        setParam (p, "drone", 0.0f);
        ed.reset();
    }

    proc.reset();
    std::cout << (failures == 0 ? "ALL OK\n" : juce::String (failures) + " FAILURE(S)\n");
    return failures == 0 ? 0 : 1;
}
