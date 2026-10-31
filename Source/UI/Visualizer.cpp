#include "Visualizer.h"
#include "Widgets.h"

namespace loom
{
Visualizer::Visualizer()
{
    for (int i = 0; i < kWaveLines; ++i) waveLines.push_back (std::vector<float> (kWavePoints, 0.0f));
}

void Visualizer::setMode (Mode m)
{
    mode = m;
    repaint();
}

void Visualizer::push (const float* l, const float* r, int n)
{
    for (int i = 0; i < n; ++i)
    {
        const int idx = (int) ((written + i) & (kRing - 1));
        ringL[(size_t) idx] = l[i];
        ringR[(size_t) idx] = r[i];
    }
    written += n;
}

void Visualizer::resized()
{
    const int h = juce::jmax (1, getHeight() - 16);
    lastSpectrum.assign ((size_t) h, 0.0f);
    peakSpectrum.assign ((size_t) h, 0.0f);
    spectrogram = juce::Image (juce::Image::RGB, juce::jmax (1, getWidth() - 24 - 110), h, true);
    spectrogram.clear (spectrogram.getBounds(), Colours::vizBg);

    const int y = 12;
    pillScope    = { getWidth() - 12 - 64, y, 64, 22 };
    pillSpectrum = { pillScope.getX() - 6 - 80, y, 80, 22 };
    pillWaves    = { pillSpectrum.getX() - 6 - 66, y, 66, 22 };
}

void Visualizer::tick()
{
    ++tickCount;
    if (mode == Waves && (tickCount % 2) == 0) captureWave();
    if (mode == Spectrum) computeSpectrumColumns();
    else spectrumRead = written;
    if (mode == Scope) computeScopeColumns();
    else scopeRead = written;
    repaint();
}

// ===========================================================================
// Waves
void Visualizer::captureWave()
{
    const int span = 2048;     // окно, которое показывает одна линия
    const int search = 1024;
    juce::int64 start = written - span - search;
    if (start < 0) return;

    // синхронизация по переходу через ноль вверх
    for (int i = 0; i < search; ++i)
    {
        const auto a = (start + i) & (kRing - 1), b = (start + i + 1) & (kRing - 1);
        const float va = ringL[(size_t) a] + ringR[(size_t) a], vb = ringL[(size_t) b] + ringR[(size_t) b];
        if (va <= 0.0f && vb > 0.0f) { start += i; break; }
    }

    std::vector<float> line ((size_t) kWavePoints);
    float peak = 0.0f;
    for (int p = 0; p < kWavePoints; ++p)
    {
        const auto idx = (start + (juce::int64) p * span / kWavePoints) & (kRing - 1);
        const float v = 0.5f * (ringL[(size_t) idx] + ringR[(size_t) idx]);
        line[(size_t) p] = v;
        peak = juce::jmax (peak, std::abs (v));
    }
    // плавная авто-нормализация, чтобы тихие патчи тоже было видно
    const float target = peak > 0.01f ? juce::jlimit (1.0f, 6.0f, 0.8f / peak) : waveGain;
    waveGain += (target - waveGain) * 0.15f;

    waveLines.push_front (std::move (line));
    while ((int) waveLines.size() > kWaveLines) waveLines.pop_back();
}

void Visualizer::paintWaves (juce::Graphics& g, juce::Rectangle<float> area)
{
    const int n = (int) waveLines.size();
    const float spacing = area.getHeight() / (float) (kWaveLines + 3);
    const float amp = spacing * 2.4f;

    // от дальних к ближним: каждая линия закрывает то, что за ней
    for (int k = n - 1; k >= 0; --k)
    {
        const auto& line = waveLines[(size_t) k];
        const float depth = (float) k / (float) (kWaveLines - 1);           // 0 — спереди
        const float baseY = area.getBottom() - spacing * 2.0f - (float) k * spacing;
        const float inset = area.getWidth() * 0.06f * depth;                  // лёгкая перспектива
        const float x0 = area.getX() + inset, x1 = area.getRight() - inset;
        const float a = amp * (1.0f - 0.35f * depth);

        juce::Path p;
        for (int i = 0; i < kWavePoints; ++i)
        {
            // края гасим, чтобы линии сходились в фон
            const float t = (float) i / (float) (kWavePoints - 1);
            const float edge = std::sin (t * juce::MathConstants<float>::pi);
            const float v = std::tanh (line[(size_t) i] * waveGain);   // мягкое ограничение вместо «срезанных» вершин
            const float y = baseY - v * a * (0.25f + 0.75f * edge);
            const float x = x0 + (x1 - x0) * t;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }

        juce::Path fill (p);
        fill.lineTo (x1, area.getBottom());
        fill.lineTo (x0, area.getBottom());
        fill.closeSubPath();
        g.setColour (Colours::panel);
        g.fillPath (fill);

        if (k == 0)
        {
            g.setColour (Colours::accent);
            g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved));
        }
        else
        {
            g.setColour (Colours::text.withAlpha (0.75f - 0.5f * depth));
            g.strokePath (p, juce::PathStrokeType (0.9f, juce::PathStrokeType::curved));
        }
    }
}

// ===========================================================================
// Spectrum
juce::Colour Visualizer::magma (float t)
{
    // приближение палитры magma: чёрный → фиолетовый → красный → оранжевый → светло-жёлтый
    static const juce::Colour stops[] = {
        juce::Colour (0xff0d0d12), juce::Colour (0xff2a0f5c), juce::Colour (0xff6a1b9a),
        juce::Colour (0xffc2185b), juce::Colour (0xfff4511e), juce::Colour (0xffffa726), juce::Colour (0xfffff3b0)
    };
    t = juce::jlimit (0.0f, 1.0f, t) * 6.0f;
    const int i = juce::jmin (5, (int) t);
    return stops[i].interpolatedWith (stops[i + 1], t - (float) i);
}

void Visualizer::computeSpectrumColumns()
{
    if (! spectrogram.isValid()) return;
    const int H = spectrogram.getHeight();
    if (written - spectrumRead > kRing / 2) spectrumRead = written - kRing / 2;

    int columns = 0;
    while (spectrumRead + kHop <= written && columns < 16)
    {
        spectrumRead += kHop;
        const juce::int64 start = spectrumRead - kFftSize;
        if (start < 0) continue;
        for (int i = 0; i < kFftSize; ++i)
        {
            const auto idx = (start + i) & (kRing - 1);
            fftBuf[(size_t) i] = 0.5f * (ringL[(size_t) idx] + ringR[(size_t) idx]);
        }
        std::fill (fftBuf.begin() + kFftSize, fftBuf.end(), 0.0f);
        window.multiplyWithWindowingTable (fftBuf.data(), (size_t) kFftSize);
        fft.performFrequencyOnlyForwardTransform (fftBuf.data(), true);

        // столбец: лог-ось частот 20 Гц .. 20 кГц, снизу вверх
        spectrogram.moveImageSection (0, 0, 1, 0, spectrogram.getWidth() - 1, H);
        const float nyq = (float) sampleRate * 0.5f;
        for (int y = 0; y < H; ++y)
        {
            const float t0 = 1.0f - (float) (y + 1) / (float) H, t1 = 1.0f - (float) y / (float) H;
            const float f0 = 20.0f * std::pow (1000.0f, t0), f1 = 20.0f * std::pow (1000.0f, t1);
            int b0 = (int) (f0 / nyq * (kFftSize / 2)), b1 = (int) (f1 / nyq * (kFftSize / 2));
            b0 = juce::jlimit (1, kFftSize / 2 - 1, b0);
            b1 = juce::jlimit (b0, kFftSize / 2 - 1, b1);
            float mag = 0.0f;
            for (int b = b0; b <= b1; ++b) mag = juce::jmax (mag, fftBuf[(size_t) b]);
            const float db = juce::Decibels::gainToDecibels (mag / (float) (kFftSize / 4), -120.0f);
            // наклон +3 дБ/окт, как в анализаторах — высокие не тонут
            const float tilt = 3.0f * std::log2 (juce::jmax (20.0f, (f0 + f1) * 0.5f) / 1000.0f);
            const float v = juce::jlimit (0.0f, 1.0f, (db + tilt + 90.0f) / 84.0f);
            spectrogram.setPixelAt (spectrogram.getWidth() - 1, y, magma (v));
            lastSpectrum[(size_t) y] = v;
        }
        ++columns;
    }
    for (size_t i = 0; i < peakSpectrum.size(); ++i)
        peakSpectrum[i] = juce::jmax (lastSpectrum[i], peakSpectrum[i] - 0.01f);
}

void Visualizer::paintSpectrum (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour (Colours::vizBg);
    g.fillRoundedRectangle (area, 8.0f);
    auto inner = area.reduced (0, 8);
    auto side = inner.removeFromRight (110.0f);
    g.drawImage (spectrogram, inner.withTrimmedLeft (8));

    // текущий спектр: горизонтальные полосы с градиентом
    const int H = (int) lastSpectrum.size();
    for (int y = 0; y < H; ++y)
    {
        const float v = lastSpectrum[(size_t) y];
        const float yy = inner.getY() + (float) y * inner.getHeight() / (float) H;
        g.setColour (magma (0.35f + 0.65f * v).withAlpha (0.9f));
        g.fillRect (side.getX() + 6, yy, (side.getWidth() - 12) * v, inner.getHeight() / (float) H + 0.5f);
        if (peakSpectrum[(size_t) y] < 0.02f) continue;
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.fillRect (side.getX() + 6 + (side.getWidth() - 12) * peakSpectrum[(size_t) y], yy, 1.0f, inner.getHeight() / (float) H + 0.5f);
    }

    // сетка частот
    g.setFont (LoomLookAndFeel::font (9.5f));
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float t = std::log (f / 20.0f) / std::log (1000.0f);
        const float y = inner.getBottom() - t * inner.getHeight();
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawHorizontalLine ((int) y, inner.getX() + 8, side.getRight() - 4);
        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.drawText (f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "k" : juce::String ((int) f),
                    juce::Rectangle<float> (side.getRight() - 30, y - 11, 26, 10), juce::Justification::centredRight);
    }
}

// ===========================================================================
// Scope
void Visualizer::computeScopeColumns()
{
    const int samplesPerCol = 96;
    if (written - scopeRead > kRing / 2) scopeRead = written - kRing / 2;
    const int maxCols = juce::jmax (1, getWidth() - 24 - 150);

    // простое 3-полосное разделение (one-pole) для окраски
    const float aLow = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * 250.0f / (float) sampleRate);
    const float aMid = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * 2500.0f / (float) sampleRate);

    while (scopeRead + samplesPerCol <= written)
    {
        ScopeCol col;
        float eL = 0, eM = 0, eH = 0;
        for (int i = 0; i < samplesPerCol; ++i)
        {
            const auto idx = (scopeRead + i) & (kRing - 1);
            const float v = 0.5f * (ringL[(size_t) idx] + ringR[(size_t) idx]);
            col.mn = juce::jmin (col.mn, v);
            col.mx = juce::jmax (col.mx, v);
            lpLow += aLow * (v - lpLow);
            lpMid += aMid * (v - lpMid);
            const float low = lpLow, mid = lpMid - lpLow, high = v - lpMid;
            eL += low * low; eM += mid * mid; eH += high * high;
        }
        scopeRead += samplesPerCol;
        const float sum = eL + eM + eH + 1e-12f;
        // низ — красный, середина — зелёный, верх — синий (как в MiniMeters)
        const float r = std::sqrt (eL / sum), gg = std::sqrt (eM / sum), b = std::sqrt (eH / sum);
        const float mx = juce::jmax (r, gg, b, 1e-6f);
        col.c = juce::Colour::fromFloatRGBA (0.15f + 0.85f * r / mx, 0.15f + 0.85f * gg / mx, 0.2f + 0.8f * b / mx, 1.0f);
        scopeCols.push_back (col);
    }
    while ((int) scopeCols.size() > maxCols) scopeCols.pop_front();

    // стереометр: последние 1024 сэмпла
    stereoPts.clear();
    for (int i = 0; i < 1024; i += 2)
    {
        const auto idx = (written - 1024 + i) & (kRing - 1);
        const float l = ringL[(size_t) idx], r = ringR[(size_t) idx];
        stereoPts.push_back ({ (r - l) * 0.7071f, (l + r) * 0.7071f });
    }
}

void Visualizer::paintScope (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour (Colours::vizBg);
    g.fillRoundedRectangle (area, 8.0f);
    auto inner = area.reduced (8);
    auto side = inner.removeFromRight (140.0f);

    // волна
    const float mid = inner.getCentreY(), hh = inner.getHeight() * 0.46f;
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawHorizontalLine ((int) mid, inner.getX(), inner.getRight());
    float x = inner.getRight() - (float) scopeCols.size();
    for (auto& c : scopeCols)
    {
        const float y0 = mid - juce::jlimit (-1.0f, 1.0f, c.mx) * hh;
        const float y1 = mid - juce::jlimit (-1.0f, 1.0f, c.mn) * hh;
        g.setColour (c.c);
        g.drawVerticalLine ((int) x, y0, juce::jmax (y0 + 1.0f, y1));
        x += 1.0f;
    }

    // стереометр (гониометр)
    auto box = side.withSizeKeepingCentre (juce::jmin (side.getWidth(), side.getHeight()) - 8, juce::jmin (side.getWidth(), side.getHeight()) - 8);
    const auto c = box.getCentre();
    const float rad = box.getWidth() * 0.5f;
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    juce::Path diamond;
    diamond.addTriangle (c.x, c.y - rad, c.x + rad, c.y, c.x, c.y + rad);
    diamond.addTriangle (c.x, c.y - rad, c.x - rad, c.y, c.x, c.y + rad);
    g.strokePath (diamond, juce::PathStrokeType (1.0f));
    g.drawLine (c.x, c.y - rad, c.x, c.y + rad, 0.6f);
    g.drawLine (c.x - rad, c.y, c.x + rad, c.y, 0.6f);
    g.setColour (juce::Colour (0xffffa726).withAlpha (0.55f));
    for (auto& p : stereoPts)
    {
        const float px = c.x + juce::jlimit (-1.0f, 1.0f, p.x) * rad;
        const float py = c.y - juce::jlimit (-1.0f, 1.0f, p.y) * rad;
        g.fillRect (px, py, 1.5f, 1.5f);
    }
    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.setFont (LoomLookAndFeel::font (9.5f));
    g.drawText ("L", box.withWidth (12).translated (-2, 0), juce::Justification::topLeft);
    g.drawText ("R", box.withTrimmedLeft (box.getWidth() - 12), juce::Justification::topRight);
}

// ===========================================================================
static void drawPill (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text, bool active, juce::Colour dot = {})
{
    auto rf = r.toFloat();
    g.setColour (active ? Colours::panel : Colours::panel.withAlpha (0.85f));
    g.fillRoundedRectangle (rf, rf.getHeight() * 0.5f);
    g.setColour (active ? Colours::outline.darker (0.2f) : Colours::outline);
    g.drawRoundedRectangle (rf.reduced (0.5f), rf.getHeight() * 0.5f, 1.0f);
    auto tr = r.reduced (10, 0);
    if (! dot.isTransparent())
    {
        g.setColour (dot);
        g.fillEllipse ((float) tr.getX(), rf.getCentreY() - 3.0f, 6.0f, 6.0f);
        tr.removeFromLeft (11);
    }
    g.setColour (active ? Colours::text : Colours::textDim);
    g.setFont (LoomLookAndFeel::font (11.0f, active));
    g.drawText (text, tr, juce::Justification::centredLeft);
}

void Visualizer::paintOverlay (juce::Graphics& g)
{
    drawPill (g, pillWaves, "Waves", mode == Waves);
    drawPill (g, pillSpectrum, "Spectrum", mode == Spectrum);
    drawPill (g, pillScope, "Scope", mode == Scope);

    auto preview = juce::Rectangle<int> (12, getHeight() - 32, 108, 22);
    drawPill (g, preview, "Result preview", false);

    juce::String drone = droneOn ? "Drone on" : "Drone off";
    if (droneOn && droneNote >= 0) drone << midDot() << juce::MidiMessage::getMidiNoteName (droneNote, true, true, 3);
    pillDrone = juce::Rectangle<int> (getWidth() - 136, getHeight() - 32, 124, 22);
    drawPill (g, pillDrone, drone, droneOn, droneOn ? Colours::good : Colours::textDim);
}

void Visualizer::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawCard (g, r, 14.0f, true);
    juce::Graphics::ScopedSaveState s (g);
    juce::Path clip;
    clip.addRoundedRectangle (r.reduced (1.0f), 14.0f);
    g.reduceClipRegion (clip);

    auto area = r.reduced (12.0f, 8.0f);
    switch (mode)
    {
        case Waves:    paintWaves (g, r.reduced (1.0f)); break;
        case Spectrum: paintSpectrum (g, area); break;
        case Scope:    paintScope (g, area); break;
    }
    paintOverlay (g);
}

void Visualizer::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    if (pillWaves.contains (p)) setMode (Waves);
    else if (pillSpectrum.contains (p)) setMode (Spectrum);
    else if (pillScope.contains (p)) setMode (Scope);
    else if (pillDrone.contains (p) && onDroneClicked) onDroneClicked();
}
} // namespace loom
