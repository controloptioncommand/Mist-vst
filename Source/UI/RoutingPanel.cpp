#include "RoutingPanel.h"
#include "../Algorithms.h"

namespace ferrite
{
static float paramValue (FerriteProcessor& p, const juce::String& id)
{
    auto* pp = p.apvts.getParameter (id);
    return pp != nullptr ? pp->convertFrom0to1 (pp->getValue()) : 0.0f;
}

Routing Routing::read (FerriteProcessor& p)
{
    Routing r;
    r.algorithm = (int) paramValue (p, "algorithm");
    r.isMatrix = r.algorithm == 0;
    for (int i = 0; i < kNumOps; ++i)
    {
        r.on[i] = paramValue (p, opId (i, "on")) > 0.5f;
        r.feedback[i] = paramValue (p, opId (i, "feedback"));
    }
    if (r.isMatrix)
    {
        for (int s = 0; s < kNumOps; ++s)
        {
            for (int d = 0; d < kNumOps; ++d)
                r.mod[s][d] = s == d ? 0.0f : paramValue (p, matId (s, d));
            r.out[s] = paramValue (p, opId (s, "out"));
        }
    }
    else
    {
        const auto& a = getAlgorithm (r.algorithm - 1);
        for (int s = 0; s < kNumOps; ++s)
        {
            for (int d = 0; d < kNumOps; ++d)
                r.mod[s][d] = a.modulates[(size_t) s][(size_t) d] ? 1.0f : 0.0f;
            r.out[s] = a.carrier[(size_t) s] ? 1.0f : 0.0f;
        }
    }
    return r;
}

// ===========================================================================
void AlgorithmDiagram::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (Colours::bg);
    g.fillRoundedRectangle (bounds, 4.0f);

    const auto r = Routing::read (proc);

    // уровень узла: 0 — выход, дальше — расстояние до выхода по «прямым» связям
    int lvl[kNumOps];
    for (int i = 0; i < kNumOps; ++i) lvl[i] = r.out[i] > 0 ? 0 : -1;
    for (int iter = 0; iter < kNumOps; ++iter)
        for (int s = 0; s < kNumOps; ++s)
            for (int d = 0; d < kNumOps; ++d)
                if (r.mod[s][d] > 0 && lvl[d] >= 0 && lvl[s] < 0)
                    lvl[s] = lvl[d] + 1;
    int maxLvl = 0;
    for (int i = 0; i < kNumOps; ++i)
    {
        if (lvl[i] < 0) lvl[i] = 0;   // висячие узлы — на нижний ряд
        maxLvl = juce::jmax (maxLvl, lvl[i]);
    }

    auto area = bounds.reduced (16.0f, 14.0f).withTrimmedBottom (14.0f);
    nodeR = juce::jlimit (9.0f, 16.0f, area.getHeight() / (float) (maxLvl + 1) * 0.28f);
    for (int L = 0; L <= maxLvl; ++L)
    {
        juce::Array<int> row;
        for (int i = 0; i < kNumOps; ++i) if (lvl[i] == L) row.add (i);
        // сортировка по средней позиции целей — меньше пересечений
        if (L > 0)
        {
            std::sort (row.begin(), row.end(), [&] (int a, int b)
            {
                auto avg = [&] (int s)
                {
                    float sum = 0; int n = 0;
                    for (int d = 0; d < kNumOps; ++d) if (r.mod[s][d] > 0 && lvl[d] < L) { sum += nodePos[(size_t) d].x; ++n; }
                    return n ? sum / (float) n : 1e6f + (float) s;
                };
                return avg (a) < avg (b);
            });
        }
        const float y = maxLvl == 0 ? area.getCentreY() : area.getBottom() - (float) L / (float) maxLvl * area.getHeight();
        for (int k = 0; k < row.size(); ++k)
            nodePos[(size_t) row[k]] = { area.getX() + area.getWidth() * ((float) k + 0.5f) / (float) row.size(), y };
    }

    // связи
    for (int s = 0; s < kNumOps; ++s)
        for (int d = 0; d < kNumOps; ++d)
        {
            if (r.mod[s][d] <= 0.0f) continue;
            const auto a = nodePos[(size_t) s], b = nodePos[(size_t) d];
            const float alpha = r.on[s] && r.on[d] ? 0.35f + 0.65f * r.mod[s][d] : 0.15f;
            g.setColour (Colours::forOperator (s).withAlpha (alpha));
            juce::Path p;
            const bool backwards = lvl[s] <= lvl[d];
            if (! backwards)
            {
                p.startNewSubPath (a.x, a.y + nodeR);
                p.lineTo (b.x, b.y - nodeR);
            }
            else
            {
                // обратная связь / петля — дуга сбоку
                const float side = a.x <= b.x ? -1.0f : 1.0f;
                const float bend = 30.0f + 6.0f * (float) std::abs (s - d);
                p.startNewSubPath (a.x + side * nodeR * 0.7f, a.y);
                p.cubicTo (a.x + side * bend, a.y + (b.y - a.y) * 0.2f - 10.0f,
                           b.x + side * bend, b.y - 10.0f, b.x + side * nodeR * 0.7f, b.y - nodeR * 0.7f);
            }
            g.strokePath (p, juce::PathStrokeType (1.0f + 1.5f * r.mod[s][d]));
            const auto end = p.getPointAlongPath (p.getLength() - 0.1f);
            const auto pre = p.getPointAlongPath (juce::jmax (0.0f, p.getLength() - 7.0f));
            juce::Path arrow;
            arrow.addArrow ({ pre, end }, 0.0f, 7.0f, 7.0f);
            g.fillPath (arrow);
        }

    // выходная шина
    const float busY = area.getBottom() + nodeR + 8.0f;
    g.setColour (Colours::textDim.withAlpha (0.6f));
    float minX = 1e9f, maxX = -1e9f;
    for (int i = 0; i < kNumOps; ++i)
        if (r.out[i] > 0)
        {
            const auto p = nodePos[(size_t) i];
            g.drawLine (p.x, p.y + nodeR, p.x, busY, 1.0f + r.out[i]);
            minX = juce::jmin (minX, p.x); maxX = juce::jmax (maxX, p.x);
        }
    if (maxX >= minX) g.drawLine (minX - 4, busY, maxX + 4, busY, 2.0f);

    // узлы
    for (int i = 0; i < kNumOps; ++i)
    {
        const auto p = nodePos[(size_t) i];
        auto c = Colours::forOperator (i);
        if (! r.on[i]) c = c.withSaturation (0.0f).withAlpha (0.5f);
        if (r.feedback[i] > 0.0f)
        {
            g.setColour (c.withAlpha (0.4f + 0.6f * r.feedback[i]));
            g.drawEllipse (p.x + nodeR * 0.3f, p.y - nodeR * 1.5f, nodeR * 1.2f, nodeR * 1.2f, 1.5f);
        }
        g.setColour (r.out[i] > 0 ? c : Colours::panelHi);
        g.fillEllipse (p.x - nodeR, p.y - nodeR, nodeR * 2, nodeR * 2);
        g.setColour (c);
        g.drawEllipse (p.x - nodeR, p.y - nodeR, nodeR * 2, nodeR * 2, 1.5f);
        g.setColour (r.out[i] > 0 ? Colours::bg : Colours::text);
        g.setFont (FerriteLookAndFeel::font (nodeR, true));
        g.drawText (juce::String (i + 1), juce::Rectangle<float> (p.x - nodeR, p.y - nodeR, nodeR * 2, nodeR * 2), juce::Justification::centred);
    }

    g.setColour (Colours::textDim);
    g.setFont (FerriteLookAndFeel::font (11.0f));
    const juce::String name = r.isMatrix ? juce::String ("MATRIX") : "ALG " + juce::String (r.algorithm) + "  " + getAlgorithm (r.algorithm - 1).name;
    g.drawText (name, bounds.reduced (6, 3), juce::Justification::topLeft);
}

void AlgorithmDiagram::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < kNumOps; ++i)
        if (e.position.getDistanceFrom (nodePos[(size_t) i]) <= nodeR + 2)
            if (onOperatorClicked) onOperatorClicked (i);
}

// ===========================================================================
juce::Rectangle<float> MatrixGrid::cellRect (int row, int col) const
{
    // столбцы: 6 целей + OUT; строки: 6 модуляторов; первая строка/столбец — подписи
    const float lw = 26.0f, th = 16.0f;
    const float cw = ((float) getWidth() - lw) / (float) (kNumOps + 1);
    const float ch = ((float) getHeight() - th) / (float) kNumOps;
    return { lw + (float) col * cw, th + (float) row * ch, cw, ch };
}

juce::RangedAudioParameter* MatrixGrid::paramFor (int row, int col) const
{
    if (col == kNumOps) return proc.apvts.getParameter (opId (row, "out"));
    if (row == col) return proc.apvts.getParameter (opId (row, "feedback"));
    return proc.apvts.getParameter (matId (row, col));
}

void MatrixGrid::paint (juce::Graphics& g)
{
    const auto r = Routing::read (proc);
    g.setFont (FerriteLookAndFeel::font (10.0f));

    for (int c = 0; c <= kNumOps; ++c)
    {
        auto rc = cellRect (0, c);
        g.setColour (c < kNumOps ? Colours::forOperator (c) : Colours::text);
        g.drawText (c < kNumOps ? juce::String (c + 1) : juce::String ("OUT"), juce::Rectangle<float> (rc.getX(), 0, rc.getWidth(), 16),
                    juce::Justification::centred);
    }
    for (int row = 0; row < kNumOps; ++row)
    {
        auto rc = cellRect (row, 0);
        g.setColour (Colours::forOperator (row));
        g.drawText (juce::String (row + 1) + ">", juce::Rectangle<float> (0, rc.getY(), 24, rc.getHeight()), juce::Justification::centred);

        for (int c = 0; c <= kNumOps; ++c)
        {
            auto cell = cellRect (row, c).reduced (1.5f);
            float v;
            if (c == kNumOps) v = r.out[row];
            else if (c == row) v = r.feedback[row];
            else v = r.mod[row][c];

            g.setColour (c == row ? Colours::panelHi.darker (0.3f) : Colours::panelHi);
            g.fillRoundedRectangle (cell, 3.0f);
            if (v > 0.0f)
            {
                const auto col = Colours::forOperator (row);
                const bool editable = r.isMatrix || c == row;
                g.setColour (col.withAlpha (editable ? 0.25f + 0.75f * v : 0.35f));
                g.fillRoundedRectangle (cell.withTrimmedTop (cell.getHeight() * (1.0f - v)), 3.0f);
            }
            if (c == row)
            {
                g.setColour (Colours::textDim);
                g.drawText ("fb", cell, juce::Justification::centred);
            }
        }
    }

    if (! r.isMatrix)
    {
        g.setColour (Colours::bg.withAlpha (0.35f));
        for (int row = 0; row < kNumOps; ++row)
            for (int c = 0; c <= kNumOps; ++c)
                if (c != row) g.fillRect (cellRect (row, c).reduced (1.5f));
    }
}

void MatrixGrid::mouseDown (const juce::MouseEvent& e)
{
    dragParam = nullptr;
    const bool matrixMode = (int) paramValue (proc, "algorithm") == 0;
    for (int row = 0; row < kNumOps; ++row)
        for (int c = 0; c <= kNumOps; ++c)
            if (cellRect (row, c).contains (e.position))
            {
                if (! matrixMode && c != row) return;   // в режиме алгоритма редактируется только feedback
                dragParam = paramFor (row, c);
                if (dragParam)
                {
                    dragStartValue = dragParam->getValue();
                    dragParam->beginChangeGesture();
                    if (e.mods.isAltDown() || e.mods.isCommandDown())
                        dragParam->setValueNotifyingHost (dragStartValue > 0 ? 0.0f : 1.0f);
                }
                return;
            }
}

void MatrixGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (dragParam == nullptr) return;
    const float sens = e.mods.isShiftDown() ? 600.0f : 150.0f;
    const float v = juce::jlimit (0.0f, 1.0f, dragStartValue - (float) e.getDistanceFromDragStartY() / sens);
    dragParam->setValueNotifyingHost (v);
    repaint();
}

void MatrixGrid::mouseUp (const juce::MouseEvent&)
{
    if (dragParam) dragParam->endChangeGesture();
    dragParam = nullptr;
}

void MatrixGrid::mouseDoubleClick (const juce::MouseEvent& e)
{
    const bool matrixMode = (int) paramValue (proc, "algorithm") == 0;
    for (int row = 0; row < kNumOps; ++row)
        for (int c = 0; c <= kNumOps; ++c)
            if (cellRect (row, c).contains (e.position) && (matrixMode || c == row))
                if (auto* p = paramFor (row, c))
                {
                    p->beginChangeGesture();
                    p->setValueNotifyingHost (p->getValue() > 0.0f ? 0.0f : 1.0f);
                    p->endChangeGesture();
                    repaint();
                }
}

// ===========================================================================
RoutingPanel::RoutingPanel (FerriteProcessor& p) : Section ("Routing"), diagram (p), proc (p), grid (p)
{
    algo.attach (proc.apvts, "algorithm");
    addAndMakeVisible (algo);
    addAndMakeVisible (prev);
    addAndMakeVisible (next);
    addAndMakeVisible (toMatrix);
    addAndMakeVisible (diagram);
    addAndMakeVisible (grid);

    hint.setText ("drag cells: amount  |  dbl-click: on/off  |  shift: fine", juce::dontSendNotification);
    hint.setFont (FerriteLookAndFeel::font (10.0f));
    hint.setColour (juce::Label::textColourId, Colours::textDim);
    addAndMakeVisible (hint);

    auto step = [this] (int d)
    {
        auto* pp = proc.apvts.getParameter ("algorithm");
        const int n = kNumAlgorithms + 1;
        const int cur = (int) pp->convertFrom0to1 (pp->getValue());
        pp->beginChangeGesture();
        pp->setValueNotifyingHost (pp->convertTo0to1 ((float) ((cur + d + n) % n)));
        pp->endChangeGesture();
    };
    prev.onClick = [step] { step (-1); };
    next.onClick = [step] { step (1); };
    toMatrix.onClick = [this] { copyAlgorithmToMatrix(); };
}

void RoutingPanel::copyAlgorithmToMatrix()
{
    const auto r = Routing::read (proc);
    if (r.isMatrix) return;
    const auto& a = getAlgorithm (r.algorithm - 1);
    const float norm = 1.0f / std::sqrt ((float) juce::jmax (1, a.numCarriers));
    auto set = [] (juce::RangedAudioParameter* p, float v)
    {
        if (p == nullptr) return;
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (v));
        p->endChangeGesture();
    };
    for (int s = 0; s < kNumOps; ++s)
    {
        for (int d = 0; d < kNumOps; ++d)
            if (s != d) set (proc.apvts.getParameter (matId (s, d)), r.mod[s][d]);
        set (proc.apvts.getParameter (opId (s, "out")), r.out[s] > 0 ? norm : 0.0f);
    }
    set (proc.apvts.getParameter ("algorithm"), 0.0f);
}

void RoutingPanel::refresh()
{
    const bool matrix = (int) paramValue (proc, "algorithm") == 0;
    toMatrix.setEnabled (! matrix);
    diagram.repaint();
    grid.repaint();
}

void RoutingPanel::resized()
{
    auto r = getContentBounds();
    auto top = r.removeFromTop (24);
    prev.setBounds (top.removeFromLeft (24));
    top.removeFromLeft (4);
    toMatrix.setBounds (top.removeFromRight (104));
    top.removeFromRight (4);
    next.setBounds (top.removeFromRight (24));
    top.removeFromRight (4);
    algo.setBounds (top);
    r.removeFromTop (6);

    hint.setBounds (r.removeFromBottom (14));
    auto left = r.removeFromLeft (r.getWidth() / 2);
    diagram.setBounds (left.reduced (0, 2));
    r.removeFromLeft (8);
    grid.setBounds (r);
}
} // namespace ferrite
