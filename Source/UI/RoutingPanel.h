#pragma once
#include "Widgets.h"
#include "../PluginProcessor.h"

namespace loom
{
// Текущая маршрутизация (алгоритм или матрица) в одном виде
struct Routing
{
    float mod[kNumOps][kNumOps] {};
    float out[kNumOps] {};
    float feedback[kNumOps] {};
    bool on[kNumOps] {};
    bool isMatrix = false;
    int algorithm = 0;   // 0 = matrix
    static Routing read (LoomProcessor& p);
};

class AlgorithmDiagram : public juce::Component
{
public:
    explicit AlgorithmDiagram (LoomProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;
    std::function<void (int)> onOperatorClicked;
    void mouseDown (const juce::MouseEvent& e) override;

private:
    LoomProcessor& proc;
    std::array<juce::Point<float>, kNumOps> nodePos {};
    float nodeR = 14.0f;
};

// Сетка 6x6 (+OUT): строки — модулятор, столбцы — цель. Тянуть по вертикали — менять значение.
class MatrixGrid : public juce::Component
{
public:
    explicit MatrixGrid (LoomProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

private:
    juce::Rectangle<float> cellRect (int row, int col) const;
    juce::RangedAudioParameter* paramFor (int row, int col) const;
    LoomProcessor& proc;
    juce::RangedAudioParameter* dragParam = nullptr;
    float dragStartValue = 0;
};

class RoutingPanel : public Section
{
public:
    explicit RoutingPanel (LoomProcessor& p);
    void resized() override;
    void refresh();
    AlgorithmDiagram diagram;

private:
    void copyAlgorithmToMatrix();
    LoomProcessor& proc;
    ParamChoice algo;
    juce::TextButton prev { "<" }, next { ">" }, toMatrix { "Edit as Matrix" };
    MatrixGrid grid;
    juce::Label hint;
};
} // namespace loom
