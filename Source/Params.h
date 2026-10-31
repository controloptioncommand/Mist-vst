#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace loom
{
constexpr int kNumOps        = 6;
constexpr int kNumVoices     = 8;
constexpr int kMaxUnison     = 8;
constexpr int kNumModSlots   = 8;
constexpr int kNumAlgorithms = 32;
constexpr int kSubBlock      = 32;   // контрольная частота модуляции (сэмплов)

inline juce::String opId (int op, const char* name)   { return "op" + juce::String (op + 1) + "_" + name; }
inline juce::String matId (int src, int dst)          { return "mat_" + juce::String (src + 1) + "_" + juce::String (dst + 1); }
inline juce::String modId (int slot, const char* name){ return "mod" + juce::String (slot + 1) + "_" + name; }
inline juce::String lfoId (int lfo, const char* name) { return "lfo" + juce::String (lfo + 1) + "_" + name; }

enum OpMode { OpModeWave = 0, OpModeSample = 1, OpModeGrain = 2 };   // Wave = «Scrub» в интерфейсе
enum Interp { InterpLinear = 0, InterpHermite = 1 };
enum LoopMode { LoopOff = 0, LoopForward = 1, LoopPingPong = 2 };
enum PolyMode { PolyPoly = 0, PolyMono = 1, PolyLegato = 2 };
enum FilterType { FiltLP12 = 0, FiltLP24, FiltBP, FiltHP, FiltNotch };
enum LfoShape { LfoSine = 0, LfoTriangle, LfoSawUp, LfoSawDown, LfoSquare, LfoSampleHold, LfoSmoothRandom };

enum ModSource
{
    SrcNone = 0, SrcLfo1, SrcLfo2, SrcModEnv, SrcVelocity, SrcKeyTrack,
    SrcModWheel, SrcAftertouch, SrcRandom, NumModSources
};

// Назначения модуляции
enum ModDest
{
    DstNone = 0, DstPitch, DstFMAmount, DstCutoff, DstReso, DstUniDetune, DstVolume, DstPan,
    DstOpLevel0,                       // +op
    DstOpPitch0    = DstOpLevel0 + kNumOps,
    DstOpScan0     = DstOpPitch0 + kNumOps,
    DstOpFeedback0 = DstOpScan0 + kNumOps,
    NumModDests    = DstOpFeedback0 + kNumOps
};

juce::StringArray modSourceNames();
juce::StringArray modDestNames();
juce::StringArray lfoDivisionNames();
double lfoDivisionInBeats (int index);
juce::StringArray algorithmChoiceNames();   // "Matrix", "1 Stack", ...

float coarseToRatio (int coarse);           // 0 -> 0.5, 1..32 -> n

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
} // namespace loom
