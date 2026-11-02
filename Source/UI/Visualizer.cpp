#include "Visualizer.h"
#include "Widgets.h"

namespace mist
{
using LnF = MistLookAndFeel;
static const char* kTabNames[] = { "Waves", "Spectrum", "Scope" };

Visualizer::Visualizer()
{
    for (int i = 0; i < kWaveLines; ++i) waveLines.push_back (std::vector<float> (kWavePoints, 0.0f));
    setWantsKeyboardFocus (true);
    setTitle ("Output view");
    setDescription ("Live view of the synth output. Tabs: Waves, Spectrum, Scope. Use left and right arrow keys to switch.");
}

juce::Colour Visualizer::background() const
{
    // Oxygen — #F5F5F2 (поверхность), Rubber — #121316 (колодец)
    return currentTheme() == ThemeId::Rubber ? Colours::well : Colours::surface;
}

void Visualizer::setMode (Mode m)
{
    mode = m;
    setDescription (juce::String ("Output view, ") + kTabNames[(int) m] + " view selected.");
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
    const float h = 28.0f;
    const float tabW = 3 * 72.0f;
    tabsArea = { (float) getWidth() - 12.0f - tabW, (float) getHeight() - 12.0f - h, tabW, h };
    legendArea = { (float) getWidth() - 12.0f - 236.0f, 12.0f, 236.0f, h };
}

void Visualizer::tick()
{
    ++tickCount;
    if (mode == Waves && (tickCount % 2) == 0) captureWave();
    if (mode == Spectrum) computeSpectrumColumns(); else spectrumRead = written;
    if (mode == Scope) computeScopeColumns(); else scopeRead = written;
    repaint();
}

// ===========================================================================
// Scope
void Visualizer::computeScopeColumns()
{
    const int samplesPerCol = 96;
    if (written - scopeRead > kRing / 2) scopeRead = written - kRing / 2;
    const int maxCols = juce::jmax (1, getWidth() - 2);

    while (scopeRead + samplesPerCol <= written)
    {
        Col c;
        float pk = 0.0f;
        for (int i = 0; i < samplesPerCol; ++i)
        {
            const auto idx = (scopeRead + i) & (kRing - 1);
            const float v = 0.5f * (ringL[(size_t) idx] + ringR[(size_t) idx]);
            c.mn = juce::jmin (c.mn, v);
            c.mx = juce::jmax (c.mx, v);
            pk = juce::jmax (pk, std::abs (v));
        }
        c.db = juce::Decibels::gainToDecibels (pk, -100.0f);
        scopeRead += samplesPerCol;
        scopeCols.push_back (c);
    }
    while ((int) scopeCols.size() > maxCols) scopeCols.pop_front();
}

void Visualizer::paintGuides (juce::Graphics& g, juce::Rectangle<float> area)
{
    // пунктирные направляющие −6 и −12 дБ (обе полярности) с подписями
    const float mid = area.getCentreY(), hh = area.getHeight() * 0.5f;
    const float dashes[] = { 3.0f, 4.0f };
    g.setFont (LnF::mono (LnF::Type::footnote));
    for (float db : { -6.0f, -12.0f })
    {
        const float a = juce::Decibels::decibelsToGain (db) * hh;
        for (float y : { mid - a, mid + a })
        {
            g.setColour (Colours::ink2.withAlpha (0.45f));
            g.drawDashedLine ({ area.getX() + 12.0f, y, area.getRight() - 48.0f, y }, dashes, 2, 1.0f);
        }
        g.setColour (Colours::ink2);
        g.drawText (juce::String::fromUTF8 ("\xe2\x88\x92") + juce::String ((int) -db),
                    juce::Rectangle<float> (area.getRight() - 44.0f, mid - a - 7.0f, 30.0f, 14.0f), juce::Justification::centredRight);
    }
}

void Visualizer::paintScope (juce::Graphics& g, juce::Rectangle<float> area)
{
    const float mid = area.getCentreY(), hh = area.getHeight() * 0.5f;
    paintGuides (g, area);

    // группируем полоски в 10 цветовых корзин
    juce::RectangleList<float> buckets[10];
    float x = area.getRight() - (float) scopeCols.size();
    for (auto& c : scopeCols)
    {
        if (x >= area.getX())
        {
            const float y0 = mid - juce::jlimit (-1.0f, 1.0f, c.mx) * hh;
            const float y1 = mid - juce::jlimit (-1.0f, 1.0f, c.mn) * hh;
            buckets[rampBucket (c.db)].addWithoutMerging ({ x, y0, 1.0f, juce::jmax (1.0f, y1 - y0) });
        }
        x += 1.0f;
    }
    for (int b = 0; b < 10; ++b)
    {
        if (buckets[b].isEmpty()) continue;
        g.setColour (rampBucketColour (Colours::carrierRamp, b));
        g.fillRectList (buckets[b]);
    }
}

// ===========================================================================
// Waves — ridgelines
void Visualizer::captureWave()
{
    const int span = 2048, search = 1024;
    juce::int64 start = written - span - search;
    if (start < 0) return;
    for (int i = 0; i < search; ++i)
    {
        const auto a = (start + i) & (kRing - 1), b = (start + i + 1) & (kRing - 1);
        if (ringL[(size_t) a] + ringR[(size_t) a] <= 0.0f && ringL[(size_t) b] + ringR[(size_t) b] > 0.0f) { start += i; break; }
    }
    std::vector<float> line ((size_t) kWavePoints);
    float peak = 0.0f;
    for (int p = 0; p < kWavePoints; ++p)
    {
        const auto idx = (start + (juce::int64) p * span / kWavePoints) & (kRing - 1);
        line[(size_t) p] = 0.5f * (ringL[(size_t) idx] + ringR[(size_t) idx]);
        peak = juce::jmax (peak, std::abs (line[(size_t) p]));
    }
    const float target = peak > 0.01f ? juce::jlimit (1.0f, 6.0f, 0.8f / peak) : waveGain;
    waveGain += (target - waveGain) * 0.15f;

    // «Уменьшить движение»: линии не уезжают вверх, обновляется только передняя
    if (Accessibility::reduceMotion()) waveLines.front() = std::move (line);
    else
    {
        waveLines.push_front (std::move (line));
        while ((int) waveLines.size() > kWaveLines) waveLines.pop_back();
    }
}

void Visualizer::paintWaves (juce::Graphics& g, juce::Rectangle<float> area)
{
    const int n = (int) waveLines.size();
    const float spacing = area.getHeight() / (float) (kWaveLines + 3);
    const float amp = spacing * 2.4f;
    const auto bg = background();

    for (int k = n - 1; k >= 0; --k)
    {
        const auto& line = waveLines[(size_t) k];
        const float depth = (float) k / (float) (kWaveLines - 1);
        const float baseY = area.getBottom() - spacing * 2.0f - (float) k * spacing;
        const float x0 = area.getX(), x1 = area.getRight();
        const float a = amp * (1.0f - 0.35f * depth);

        juce::Path p;
        for (int i = 0; i < kWavePoints; ++i)
        {
            const float t = (float) i / (float) (kWavePoints - 1);
            const float edge = std::sin (t * juce::MathConstants<float>::pi);
            const float v = std::tanh (line[(size_t) i] * waveGain);
            const float x = x0 + (x1 - x0) * t, y = baseY - v * a * (0.25f + 0.75f * edge);
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        juce::Path fill (p);
        fill.lineTo (x1, area.getBottom());
        fill.lineTo (x0, area.getBottom());
        fill.closeSubPath();
        g.setColour (bg);
        g.fillPath (fill);

        if (k == 0) { g.setColour (Colours::signal); g.strokePath (p, juce::PathStrokeType (1.8f)); }
        else        { g.setColour (Colours::ink.withAlpha (0.7f - 0.45f * depth)); g.strokePath (p, juce::PathStrokeType (1.0f)); }
    }
}

// ===========================================================================
// Spectrum
// Оптимизация: спектрограмма — это картинка, которая сдвигается влево; на каждом тике дорисовываются
// только новые столбцы (через BitmapData), цвета — из таблицы на 256 значений.
void Visualizer::prepareSpectrum()
{
    const int rows = juce::jmax (8, getHeight() - 18);
    const int width = juce::jmax (8, getWidth() - 2);
    if (rows != specRows || sampleRate != specRate)
    {
        specRows = rows;
        specRate = sampleRate;
        binRange.resize ((size_t) rows);
        tiltDb.resize ((size_t) rows);
        const float nyq = (float) sampleRate * 0.5f;
        for (int y = 0; y < rows; ++y)
        {
            const float t0 = 1.0f - (float) (y + 1) / (float) rows, t1 = 1.0f - (float) y / (float) rows;
            const float f0 = 20.0f * std::pow (1000.0f, t0), f1 = 20.0f * std::pow (1000.0f, t1);
            const int b0 = juce::jlimit (1, kFftSize / 2 - 1, (int) (f0 / nyq * (kFftSize / 2)));
            const int b1 = juce::jlimit (b0, kFftSize / 2 - 1, (int) (f1 / nyq * (kFftSize / 2)));
            binRange[(size_t) y] = { b0, b1 };
            tiltDb[(size_t) y] = 3.0f * std::log2 (juce::jmax (20.0f, (f0 + f1) * 0.5f) / 1000.0f);
        }
        spectroCols.clear();
        specImage = {};
    }
    if (! specImage.isValid() || specImage.getWidth() != width || specImage.getHeight() != rows)
    {
        specImage = juce::Image (juce::Image::ARGB, width, rows, true);
        lutKey = {};   // перерисовать из истории
    }
}

void Visualizer::updateLut()
{
    const auto bg = background();
    const auto key = bg.toString() + Colours::carrierRamp[0].toString() + Colours::carrierRamp[1].toString() + Colours::carrierRamp[2].toString();
    if (key == lutKey) return;
    lutKey = key;
    for (int i = 0; i < 256; ++i)
    {
        const float v = (float) i / 255.0f;
        const auto c = v < 0.05f ? juce::Colours::transparentBlack
                                 : bg.interpolatedWith (rampColour (Colours::carrierRamp, v), juce::jmin (1.0f, v * 1.8f));
        lut[(size_t) i] = c.getPixelARGB();
    }
    // тема сменилась — перерисовать картинку из сохранённых значений
    specImage.clear (specImage.getBounds());
    const int n = (int) spectroCols.size();
    for (int k = 0; k < n; ++k)
        drawSpectrumColumn (specImage.getWidth() - (n - k) * kSpecColW, spectroCols[(size_t) k]);
}

void Visualizer::drawSpectrumColumn (int x, const std::vector<juce::uint8>& col)
{
    if (x + kSpecColW <= 0 || x >= specImage.getWidth()) return;
    juce::Image::BitmapData bd (specImage, juce::Image::BitmapData::writeOnly);
    const int rows = juce::jmin ((int) col.size(), specImage.getHeight());
    for (int y = 0; y < rows; ++y)
    {
        const auto px = lut[col[(size_t) y]];
        for (int dx = 0; dx < kSpecColW; ++dx)
            if (x + dx >= 0 && x + dx < specImage.getWidth())
                bd.setPixelColour (x + dx, y, juce::Colour (px));
    }
}

void Visualizer::computeSpectrumColumns()
{
    prepareSpectrum();
    updateLut();
    if (written - spectrumRead > kRing / 2) spectrumRead = written - kRing / 2;
    const int rows = specRows;
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

        std::vector<juce::uint8> col ((size_t) rows);
        const float norm = 1.0f / (float) (kFftSize / 4);
        for (int y = 0; y < rows; ++y)
        {
            const auto [b0, b1] = binRange[(size_t) y];
            float mag = 0.0f;
            for (int b = b0; b <= b1; ++b) mag = juce::jmax (mag, fftBuf[(size_t) b]);
            const float db = juce::Decibels::gainToDecibels (mag * norm, -120.0f);
            col[(size_t) y] = (juce::uint8) juce::roundToInt (juce::jlimit (0.0f, 1.0f, (db + tiltDb[(size_t) y] + 90.0f) / 84.0f) * 255.0f);
        }

        // сдвигаем картинку влево и рисуем только новый столбец
        const int w = specImage.getWidth();
        specImage.moveImageSection (0, 0, kSpecColW, 0, w - kSpecColW, specImage.getHeight());
        drawSpectrumColumn (w - kSpecColW, col);
        spectroCols.push_back (std::move (col));
        ++columns;
    }
    while ((int) spectroCols.size() > specImage.getWidth() / kSpecColW + 1) spectroCols.pop_front();
}

void Visualizer::paintSpectrum (juce::Graphics& g, juce::Rectangle<float> area)
{
    prepareSpectrum();
    updateLut();
    g.drawImage (specImage, area, juce::RectanglePlacement::stretchToFit);
    g.setFont (LnF::mono (LnF::Type::footnote));
    for (float f : { 100.0f, 1000.0f, 10000.0f })
    {
        const float t = std::log (f / 20.0f) / std::log (1000.0f);
        const float y = area.getBottom() - t * area.getHeight();
        g.setColour (Colours::ink2);
        g.drawText (f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "k" : juce::String ((int) f),
                    juce::Rectangle<float> (area.getX() + 4.0f, y - 7.0f, 40.0f, 14.0f), juce::Justification::centredLeft);
    }
}

// ===========================================================================
void Visualizer::paintOverlay (juce::Graphics& g)
{
    // легенда
    // плашки на виде: если фон вида Surface — плашки Well, и наоборот
    const auto plateFill = currentTheme() == ThemeId::Rubber ? Colours::surface : Colours::well;
    LnF::drawPlate (g, legendArea, 8.0f, plateFill);
    {
        auto r = legendArea.reduced (12.0f, 0.0f);
        g.setFont (LnF::mono (LnF::Type::footnote));
        g.setColour (Colours::ink);
        g.drawText (juce::String::fromUTF8 ("\xe2\x88\x92" "24 dB"), r.removeFromLeft (46.0f), juce::Justification::centredLeft);
        g.drawText ("0 dB", r.removeFromRight (30.0f), juce::Justification::centredRight);
        auto bar = r.reduced (6.0f, 0.0f).withSizeKeepingCentre (r.getWidth() - 12.0f, 6.0f);
        const float seg = bar.getWidth() / 10.0f;
        for (int b = 0; b < 10; ++b)
        {
            g.setColour (rampBucketColour (Colours::carrierRamp, b));
            g.fillRect (juce::Rectangle<float> (bar.getX() + seg * (float) b + 0.5f, bar.getY(), seg - 1.0f, bar.getHeight()));
        }
    }

    // стеклянные вкладки вида
    LnF::drawPlate (g, tabsArea, tabsArea.getHeight() * 0.5f, plateFill);   // сегменты — капсула
    const float w = (tabsArea.getWidth() - 4.0f) / 3.0f;
    for (int i = 0; i < 3; ++i)
    {
        auto cell = juce::Rectangle<float> (tabsArea.getX() + 2.0f + w * (float) i, tabsArea.getY() + 2.0f, w, tabsArea.getHeight() - 4.0f);
        const bool sel = (int) mode == i;
        if (sel)
        {
            g.setColour (currentTheme() == ThemeId::Rubber ? Colours::well : Colours::surface);
            g.fillRoundedRectangle (cell, cell.getHeight() * 0.5f);
            g.setColour (Colours::outline);
            g.drawRoundedRectangle (cell.reduced (0.5f), cell.getHeight() * 0.5f, 1.0f);
        }
        g.setColour (sel ? Colours::ink : Colours::ink2);
        g.setFont (LnF::font (LnF::Type::subheadline, sel));
        g.drawText (kTabNames[i], cell, juce::Justification::centred);
    }
    if (hasKeyboardFocus (false))
        LnF::drawFocusRing (g, tabsArea.expanded (2.0f), tabsArea.getHeight() * 0.5f + 2.0f);
}

void Visualizer::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float radius = 12.0f;
    g.setColour (background());
    g.fillRoundedRectangle (r, radius);
    {
        juce::Graphics::ScopedSaveState s (g);
        juce::Path clip;
        clip.addRoundedRectangle (r.reduced (1.0f), radius - 1.0f);
        g.reduceClipRegion (clip);
        auto area = r.reduced (1.0f, 46.0f);
        switch (mode)
        {
            case Scope:    paintScope (g, area); break;
            case Waves:    paintWaves (g, r.reduced (1.0f).withTrimmedTop (36.0f)); break;
            case Spectrum: paintSpectrum (g, r.reduced (1.0f, 8.0f)); break;
        }
        paintOverlay (g);
    }
    g.setColour (Colours::outline);
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
}

void Visualizer::mouseDown (const juce::MouseEvent& e)
{
    if (tabsArea.contains (e.position))
    {
        const int i = juce::jlimit (0, 2, (int) ((e.position.x - tabsArea.getX()) / tabsArea.getWidth() * 3.0f));
        setMode ((Mode) i);
    }
}

bool Visualizer::keyPressed (const juce::KeyPress& k)
{
    if (k.getKeyCode() == juce::KeyPress::leftKey)  { setMode ((Mode) juce::jmax (0, (int) mode - 1)); return true; }
    if (k.getKeyCode() == juce::KeyPress::rightKey) { setMode ((Mode) juce::jmin (2, (int) mode + 1)); return true; }
    return false;
}
} // namespace mist
