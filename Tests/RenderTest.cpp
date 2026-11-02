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

struct Stats { float peak = 0; double rms = 0; bool finite = true; double realtimeFactor = 0; double rmsWindow = 0; };

static Stats render (MistProcessor& p, const std::vector<int>& notes, double holdSec, double tailSec,
                     const juce::File& out, double sr = 48000.0, int block = 512, double winFrom = 0.0, double winTo = 0.0)
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
    if (winTo > winFrom)
    {
        const int a = juce::jlimit (0, total, (int) (winFrom * sr)), b = juce::jlimit (0, total, (int) (winTo * sr));
        double ws = 0;
        for (int c = 0; c < 2; ++c) for (int i = a; i < b; ++i) ws += (double) all.getSample (c, i) * all.getSample (c, i);
        st.rmsWindow = std::sqrt (ws / juce::jmax (1.0, 2.0 * (b - a)));
    }

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

// Разные «чужие» семплы для шаблонов: шум, щелчок, тихое начало, аккорд синусов, очень короткий
static juce::File writeMono (const juce::File& dir, const juce::String& name, const juce::AudioBuffer<float>& b, double sr)
{
    auto f = dir.getChildFile (name + ".wav");
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os = std::make_unique<juce::FileOutputStream> (f);
    if (auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions{}.withSampleRate (sr).withNumChannels (1).withBitsPerSample (24)))
        w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    return f;
}

static std::vector<juce::File> makeTemplateSamples (const juce::File& dir)
{
    const double sr = 48000.0;
    const double twoPi = juce::MathConstants<double>::twoPi;
    std::vector<juce::File> out;
    juce::Random rng (11);
    {   // белый шум 1.5 с
        juce::AudioBuffer<float> b (1, (int) (1.5 * sr));
        for (int i = 0; i < b.getNumSamples(); ++i) b.setSample (0, i, rng.nextFloat() * 1.6f - 0.8f);
        out.push_back (writeMono (dir, "tpl_noise", b, sr));
    }
    {   // щелчок/удар: затухающий синус 900 Гц, дальше тишина
        juce::AudioBuffer<float> b (1, (int) (1.0 * sr));
        for (int i = 0; i < b.getNumSamples(); ++i) b.setSample (0, i, (float) (std::sin (twoPi * 900.0 * i / sr) * std::exp (-i / (0.02 * sr))));
        out.push_back (writeMono (dir, "tpl_click", b, sr));
    }
    {   // 0.5 с тишины, затем аккорд синусов (A2 + E3 + A3)
        juce::AudioBuffer<float> b (1, (int) (2.0 * sr));
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const double t = i / sr - 0.5;
            const double v = t < 0 ? 0.0 : 0.3 * (std::sin (twoPi * 110.0 * t) + std::sin (twoPi * 164.8 * t) + std::sin (twoPi * 220.0 * t));
            b.setSample (0, i, (float) v);
        }
        out.push_back (writeMono (dir, "tpl_late_chord", b, sr));
    }
    {   // очень короткий: 30 мс пилы 440 Гц
        juce::AudioBuffer<float> b (1, (int) (0.03 * sr));
        double ph = 0;
        for (int i = 0; i < b.getNumSamples(); ++i) { ph += 440.0 / sr; ph -= std::floor (ph); b.setSample (0, i, (float) (2 * ph - 1) * 0.7f); }
        out.push_back (writeMono (dir, "tpl_tiny", b, sr));
    }
    return out;
}

static bool loadAndWait (MistProcessor& p, int op, const juce::File& f)
{
    const int before = p.getSampleVersion();
    p.loadSampleAsync (op, f);
    for (int i = 0; i < 300 && p.getSampleVersion() == before; ++i)
        juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
    return p.getSample (op) != nullptr;
}

static float paramValue (MistProcessor& p, const juce::String& id)
{
    auto* pp = p.apvts.getParameter (id);
    return pp->convertFrom0to1 (pp->getValue());
}

static int findPreset (MistProcessor& p, const juce::String& name)
{
    p.presets.refresh();
    const auto& e = p.presets.getEntries();
    for (int i = 0; i < e.size(); ++i) if (e[i].name == name) return i;
    return 0;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir = argc > 1 ? juce::File (juce::String (argv[1])) : juce::File::getCurrentWorkingDirectory().getChildFile ("render");
    outDir.createDirectory();

    auto proc = std::make_unique<MistProcessor>();
    auto& p = *proc;

    std::cout << "== Templates (100 factory presets), empty and with foreign samples in Op 1\n";
    const auto& fp = getFactoryPresets();
    CHECK (fp.size() == 100, "expected 100 templates, got " + juce::String ((int) fp.size()));
    {
        juce::StringArray names;
        for (auto& t : fp)
        {
            CHECK (! names.contains (t.name), "duplicate template name " + t.name);
            names.add (t.name);
            CHECK (t.name.startsWith (t.category + " "), "template name must start with its character: " + t.name);
            CHECK (getTemplateCategories().contains (t.category), "unknown character " + t.category);
            for (auto& [id, v] : t.values)
                CHECK (p.apvts.getParameter (id) != nullptr, t.name + ": unknown parameter " + id);
        }
    }
    p.presets.refresh();
    // семплы и WAV шаблонов — в подпапке: CI кладёт в архив релиза только WAV из корня папки рендера
    const auto tplDir = outDir.getChildFile ("templates");
    tplDir.createDirectory();
    auto tplSamples = makeTemplateSamples (tplDir);
    tplSamples.insert (tplSamples.begin(), makeTestSample (outDir));
    {
        // проходы: без семпла, затем с каждым из 5 разных семплов в Op 1
        std::map<juce::String, std::pair<double, int>> loudness;   // характер -> сумма dB, число
        int silent = 0, broken = 0;
        float worstPeak = 0;
        for (int pass = -1; pass < (int) tplSamples.size(); ++pass)
        {
            p.presets.initPatch();
            if (pass >= 0) CHECK (loadAndWait (p, kTemplateSampleOp, tplSamples[(size_t) pass]), "template sample did not load");
            for (int i = 0; i < (int) fp.size(); ++i)
            {
                p.presets.load (i);
                if (pass >= 0) CHECK (p.getSample (kTemplateSampleOp) != nullptr, fp[(size_t) i].name + " dropped the sample");
                const bool write = pass == -1 && i % 10 == 0;   // по одному WAV на характер
                auto st = render (p, { 48, 55, 60, 64 }, 1.2, 0.8, write ? tplDir.getChildFile ("template_" + fp[(size_t) i].name.replace (" ", "_") + ".wav") : juce::File());
                worstPeak = juce::jmax (worstPeak, st.peak);
                if (! st.finite || st.peak > 1.0f) { ++broken; report (fp[(size_t) i].name + " pass " + juce::String (pass), st); }
                if (st.rms < 2e-4)
                {
                    ++silent;
                    std::cout << "  silent: " << fp[(size_t) i].name << " pass " << pass << " rms " << st.rms << "\n";
                }
                auto& l = loudness[fp[(size_t) i].category];
                l.first += juce::Decibels::gainToDecibels ((float) st.rms, -100.0f);
                l.second += 1;
            }
        }
        for (auto& c : getTemplateCategories())
            std::cout << "  " << c.paddedRight (' ', 10) << " mean rms " << juce::String (loudness[c].first / juce::jmax (1, loudness[c].second), 1) << " dB\n";
        std::cout << "  worst peak " << worstPeak << ", silent " << silent << ", broken " << broken << "\n";
        CHECK (silent == 0, "some templates are silent");
        CHECK (broken == 0, "some templates produced NaN or clipped");
    }

    if (juce::SystemStats::getEnvironmentVariable ("MIST_TEMPLATE_LEVELS", {}).isNotEmpty())
    {
        // для выравнивания громкости шаблонов: аккорд 9 с, без семпла и с пилой.
        // «steady» — RMS на 7–9 с (установившийся звук), «early» — на 0–2 с (для ударных)
        for (int pass = -1; pass < 1; ++pass)
        {
            p.presets.initPatch();
            if (pass >= 0) loadAndWait (p, kTemplateSampleOp, tplSamples[0]);
            for (int i = 0; i < (int) fp.size(); ++i)
            {
                p.presets.load (i);
                setParam (p, "master", -6.0f);   // мерим без текущей поправки
                auto steady = render (p, { 48, 55, 60, 64 }, 9.0, 0.0, {}, 48000.0, 512, 7.0, 9.0);
                auto early = render (p, { 48, 55, 60, 64 }, 2.0, 0.0, {}, 48000.0, 512, 0.0, 2.0);
                std::cout << "  level " << pass << " " << fp[(size_t) i].name << " " << juce::Decibels::gainToDecibels ((float) steady.rmsWindow, -100.0f)
                          << " " << juce::Decibels::gainToDecibels ((float) early.rmsWindow, -100.0f) << " " << juce::jmax (steady.peak, early.peak) << "\n";
            }
        }
    }

    std::cout << "== Template keeps the sample, its settings, slot names and Transpose\n";
    {
        p.presets.initPatch();
        CHECK (loadAndWait (p, 0, tplSamples[0]), "sample did not load");
        CHECK (loadAndWait (p, 3, tplSamples[1]), "sample did not load");
        setParam (p, opId (0, "loopStart"), 0.3f);
        setParam (p, opId (0, "reverse"), 1.0f);
        setParam (p, opId (0, "root"), 50.0f);
        setParam (p, "transpose", 5.0f);
        p.setOpName (0, "Voice");
        p.presets.load (findPreset (p, "Warm Brass"));
        CHECK (p.getSample (0) != nullptr && p.getSample (3) != nullptr, "template removed samples");
        CHECK (std::abs (paramValue (p, opId (0, "loopStart")) - 0.3f) < 0.01f, "template changed loop start");
        CHECK (paramValue (p, opId (0, "reverse")) > 0.5f, "template changed reverse");
        CHECK ((int) paramValue (p, opId (0, "root")) == 50, "template changed root");
        CHECK ((int) paramValue (p, "transpose") == 5, "template changed transpose");
        CHECK (p.getOpName (0) == "Voice", "template changed slot name");
        CHECK ((int) paramValue (p, "algorithm") == 3, "template did not apply its algorithm");
        p.presets.initPatch();
        CHECK (p.getSample (0) == nullptr && p.getOpName (0) == "Op 1", "Init patch must clear samples and names");
        CHECK ((int) paramValue (p, "transpose") == 0, "Init patch must reset transpose");
    }

    std::cout << "== Slot names survive state save/restore\n";
    {
        p.presets.initPatch();
        p.setOpName (2, "  Kick drum with a very long name  ");
        CHECK (p.getOpName (2) == juce::String ("Kick drum with a very long name").substring (0, MistProcessor::kMaxOpNameLength).trim()
               || p.getOpName (2).length() <= MistProcessor::kMaxOpNameLength, "name not trimmed");
        p.setOpName (4, "Pad");
        juce::MemoryBlock st;
        p.getStateInformation (st);
        auto p2 = std::make_unique<MistProcessor>();
        p2->setStateInformation (st.getData(), (int) st.getSize());
        CHECK (p2->getOpName (4) == "Pad", "slot name not restored");
        CHECK (p2->getOpName (0) == "Op 1", "default slot name wrong");
        p.setOpName (4, "");
        CHECK (p.getOpName (4) == "Op 5", "empty name must restore default");

        // пресет из v2 (без новых параметров) не наследует изгибы и Transpose от предыдущего звука
        auto xml = p.createStateXml (false);
        for (auto* id : { "transpose", "hold", "op1_aCurve", "op1_dCurve", "op1_rCurve", "menv_aCurve" })
            for (auto* c : xml->getChildIterator())
                if (c->getStringAttribute ("id") == id) { xml->removeChildElement (c, true); break; }
        setParam (p, "transpose", 7.0f);
        setParam (p, opId (0, "aCurve"), 0.9f);
        setParam (p, opId (0, "dCurve"), -0.9f);
        p.restoreStateXml (*xml);
        CHECK ((int) paramValue (p, "transpose") == 0, "old preset kept transpose from previous sound");
        CHECK (std::abs (paramValue (p, opId (0, "aCurve")) - kDefaultAttackCurve) < 0.01f, "old preset kept attack curve");
        CHECK (std::abs (paramValue (p, opId (0, "dCurve")) - kDefaultFallCurve) < 0.01f, "old preset kept decay curve");
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

        // по умолчанию кубик не трогает панораму и режимы операторов
        p.presets.initPatch();
        setParam (p, opId (0, "pan"), 0.42f);
        setParam (p, "uniSpread", 0.13f);
        for (int k = 0; k < 5; ++k) randomizePatch (p, rng);
        CHECK (std::abs (paramValue (p, opId (0, "pan")) - 0.42f) < 0.01f, "dice changed pan");
        CHECK (std::abs (paramValue (p, "uniSpread") - 0.13f) < 0.01f, "dice changed stereo spread");
        CHECK ((int) paramValue (p, opId (0, "mode")) == OpModeWave, "dice changed operator mode");

        // запрещённые группы не меняются, разрешённые режимы — меняются только у операторов с семплом
        RandomizerOptions o;
        o.enabled.fill (false);
        o.enabled[RandomizerOptions::OperatorModes] = true;
        CHECK (loadAndWait (p, 2, tplSamples[0]), "sample did not load");
        const float cutoffBefore = paramValue (p, "cutoff");
        const int algoBefore = (int) paramValue (p, "algorithm");
        bool modeChanged = false;
        for (int k = 0; k < 12; ++k)
        {
            randomizePatch (p, rng, o);
            modeChanged |= (int) paramValue (p, opId (2, "mode")) != OpModeWave;
            CHECK ((int) paramValue (p, opId (0, "mode")) == OpModeWave, "dice changed mode of an operator without sample");
        }
        CHECK (modeChanged, "dice never changed the mode of an operator with a sample");
        CHECK (std::abs (paramValue (p, "cutoff") - cutoffBefore) < 1.0f, "locked filter changed");
        CHECK ((int) paramValue (p, "algorithm") == algoBefore, "locked algorithm changed");
    }

    std::cout << "== Transpose, envelope curves, Hold\n";
    {
        // Transpose +12 на ноте 48 == та же нота 60 без транспонирования
        p.presets.initPatch();
        setParam (p, "transpose", 12.0f);
        auto a = render (p, { 48 }, 0.5, 0.1, outDir.getChildFile ("transpose_a.wav"));
        setParam (p, "transpose", 0.0f);
        auto b = render (p, { 60 }, 0.5, 0.1, outDir.getChildFile ("transpose_b.wav"));
        CHECK (std::abs (a.rms - b.rms) < 0.02 * b.rms, "transpose +12 differs from an octave up");
        outDir.getChildFile ("transpose_a.wav").deleteFile();
        outDir.getChildFile ("transpose_b.wav").deleteFile();

        // крайние изгибы огибающих: конечные значения, звучит
        for (float c : { -1.0f, 0.0f, 1.0f })
        {
            p.presets.initPatch();
            for (int i = 0; i < kNumOps; ++i)
            {
                setParam (p, opId (i, "aCurve"), c); setParam (p, opId (i, "dCurve"), -c); setParam (p, opId (i, "rCurve"), c);
                setParam (p, opId (i, "attack"), 0.3f); setParam (p, opId (i, "release"), 0.3f);
            }
            setParam (p, opId (1, "level"), 0.5f);
            auto st = render (p, { 60 }, 0.8, 0.5, {});
            report ("curves " + juce::String (c, 1), st);
            CHECK (st.rms > 1e-3, "curve patch silent");
        }

        // Hold: отпущенные ноты звучат; новое нажатие заменяет их; аккорд держится целиком
        p.presets.initPatch();
        setParam (p, opId (0, "release"), 0.01f);
        setParam (p, "hold", 1.0f);
        p.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> buf (2, 512);
        auto blockWith = [&] (std::initializer_list<juce::MidiMessage> msgs, int blocks)
        {
            for (int k = 0; k < blocks; ++k)
            {
                juce::MidiBuffer midi;
                if (k == 0) for (auto& m : msgs) midi.addEvent (m, 0);
                p.processBlock (buf, midi);
            }
        };
        using MM = juce::MidiMessage;
        blockWith ({ MM::noteOn (1, 60, (juce::uint8) 100), MM::noteOn (1, 64, (juce::uint8) 100), MM::noteOn (1, 67, (juce::uint8) 100) }, 2);
        blockWith ({ MM::noteOff (1, 60), MM::noteOff (1, 64), MM::noteOff (1, 67) }, 10);
        CHECK (p.activeVoices.load() == 3, "Hold did not keep the chord: " + juce::String (p.activeVoices.load()));
        blockWith ({ MM::noteOn (1, 50, (juce::uint8) 100) }, 2);
        blockWith ({ MM::noteOff (1, 50) }, 20);
        CHECK (p.activeVoices.load() == 1, "Hold: new press did not replace held notes: " + juce::String (p.activeVoices.load()));
        setParam (p, "hold", 0.0f);
        blockWith ({}, 20);
        CHECK (p.activeVoices.load() == 0, "Hold off did not release");

        // Drone копит ноты
        setParam (p, "drone", 1.0f);
        blockWith ({ MM::noteOn (1, 60, (juce::uint8) 100) }, 2);
        blockWith ({ MM::noteOff (1, 60) }, 2);
        blockWith ({ MM::noteOn (1, 62, (juce::uint8) 100) }, 2);
        blockWith ({ MM::noteOff (1, 62) }, 20);
        CHECK (p.activeVoices.load() == 2, "Drone did not accumulate notes");
        setParam (p, "drone", 0.0f);
        blockWith ({}, 20);
        CHECK (p.activeVoices.load() == 0, "Drone off did not release");
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
        p.presets.load (findPreset (p, "Airy Swarm"));   // Op1 — несущая, Op2 — модулятор
        if (asset)
        {
            p.loadSampleAsync (0, sampleFile);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
            p.loadSampleAsync (1, sampleFile);
        }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (500);
        setParam (p, "drone", 1.0f);
        setParam (p, opId (0, "mode"), (float) OpModeWave);
        setParam (p, opId (0, "aCurve"), 0.6f);
        setParam (p, opId (0, "rCurve"), 0.85f);
        p.setOpName (1, "Texture");
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
            le->setView (1, 2, 0);  pump (10, false); shoot ("ui_" + tn + "_envelope");
            le->setView (4, 2, 0);  pump (10, false); shoot ("ui_" + tn + "_filter");
            le->setView (6, 1, 0);
            if (theme == ThemeId::Rubber && juce::SystemStats::getEnvironmentVariable ("MIST_MANUAL_SHOTS", {}).isNotEmpty())
            {
                // скриншоты всех вкладок для мануала (docs/manual/build.sh)
                for (int pg = 0; pg < 7; ++pg) { le->setView (pg, 2, pg == 2 ? 1 : 0); pump (20, false); shoot ("page" + juce::String (pg) + "_" + tn); }
                le->setView (6, 1, 0);
            }
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
            {
                // «Туман»: сдуваем курсором, кадр в полёте, затем слово должно собраться за ~1 с
                auto& fog = le->getAboutForTest().getFogForTest();
                std::cout << "  fog particles: " << fog.getNumParticles() << "\n";
                CHECK (fog.getNumParticles() > 150, "fog: too few particles");
                const auto c = fog.getLocalBounds().getCentre().toFloat();
                fog.blowForTest ({ c.x - 160.0f, c.y }, { c.x + 40.0f, c.y + 10.0f }, 12);
                CHECK (fog.isAnimating(), "fog: did not start");
                fog.stepForTest (8);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
                shoot ("about_" + tn + "_fog");
                fog.stepForTest (90);
                CHECK (! fog.isAnimating(), "fog: did not settle within ~1.5 s");
                juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
                shoot ("about_" + tn + "_settled");
            }
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
