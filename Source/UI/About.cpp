#include "About.h"

namespace mist
{
using LnF = MistLookAndFeel;

// ===========================================================================
// Сова 12×13. o — тело/крылья, w — лицо и грудка, e — глаза, b — клюв, c — коготки
namespace
{
const char* owlBody[11] = {
    "..o......o..",
    ".oooooooooo.",
    "oowwwwwwwwoo",
    "owweewweewwo",
    "owweewweewwo",
    "oowwwbbwwwoo",
    "ooowwwwwwooo",
    "oooowwwwoooo",
    "oooowwwwoooo",
    ".ooowwwwooo.",
    "..oooooooo..",
};
const char* owlLegs[4][2] = {
    { "...o....o...", "..cc....cc.." },   // бег, кадр 1
    { "....o..o....", "...cc..cc..." },   // бег, кадр 2
    { "...oo..oo...", "............" },   // прыжок: лапы поджаты
    { ".oooooooooo.", "..cc....cc.." },   // сидит
};

juce::Colour owlColour (char ch)
{
    const bool rubber = currentTheme() == ThemeId::Rubber;
    switch (ch)
    {
        case 'o': return Colours::signal;
        case 'w': return rubber ? Colours::ink : Colours::surface;
        case 'e': return Colours::onSignal;
        case 'b':
        case 'c': return rubber ? Colours::track : Colours::ink2;
        default:  return {};
    }
}
} // namespace

void drawOwl (juce::Graphics& g, juce::Point<int> o, int u, int frame)
{
    frame = juce::jlimit (0, 3, frame);
    const int sitShift = frame == 3 ? 1 : 0;   // сидит — на клетку ниже
    auto row = [&] (const char* s, int y)
    {
        for (int x = 0; x < 12; ++x)
            if (s[x] != '.')
            {
                g.setColour (owlColour (s[x]));
                g.fillRect (o.x + x * u, o.y + (y + sitShift) * u, u, u);
            }
    };
    for (int y = 0; y < 11; ++y) row (owlBody[y], y);
    row (owlLegs[frame][0], 11);
    if (frame != 3) row (owlLegs[frame][1], 12);
}

// ===========================================================================
GameView::GameView()
{
    setWantsKeyboardFocus (true);
    setTitle ("Owl runner");
    setDescription ("A pixel barn owl runs and jumps over stumps and a haystack. Space, up arrow or tap to jump.");
    reset();
}

void GameView::reset()
{
    obstacles.clear();
    owlY = velY = 0.0f;
    speed = 1.6f;
    score = 0.0f;
    sinceLast = 0.0f;
    nextGap = 70.0f;
    state = State::Ready;
}

void GameView::start()
{
    reset();
    startTimerHz (60);
    grabKeyboardFocus();
    repaint();
}

void GameView::stop()
{
    stopTimer();
    reset();
}

void GameView::jump()
{
    if (state == State::Over) { reset(); state = State::Running; return; }
    if (state == State::Ready) state = State::Running;
    if (owlY <= 0.001f) velY = 4.4f;
}

bool GameView::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::spaceKey || k == juce::KeyPress::upKey) { jump(); return true; }
    return false;
}

void GameView::spawnObstacle()
{
    obstacles.push_back ({ (float) kGridW + 2.0f, rng.nextInt (4) == 0 ? 1 : 0 });
    sinceLast = 0.0f;
    nextGap = (55.0f + rng.nextFloat() * 60.0f) * std::sqrt (speed / 1.6f);
}

void GameView::timerCallback()
{
    ++frame;
    if (state == State::Running)
    {
        // физика: единицы — клетки сетки, кадр — 1/60 с
        velY -= 0.36f;
        owlY = juce::jmax (0.0f, owlY + velY);
        if (owlY <= 0.0f) velY = 0.0f;

        speed = juce::jmin (4.2f, speed + 0.0009f);   // скорость растёт плавно
        score += speed * 0.1f;
        pebbleOffset += speed;
        sinceLast += speed;
        for (auto& o : obstacles) o.x -= speed;
        obstacles.erase (std::remove_if (obstacles.begin(), obstacles.end(), [] (const Obstacle& o) { return o.x < -14.0f; }),
                         obstacles.end());
        if (sinceLast > nextGap) spawnObstacle();

        // столкновение (с небольшим допуском)
        const float owlL = (float) kOwlX + 2.0f, owlR = (float) kOwlX + 10.0f, owlBottom = owlY + 1.0f;
        for (auto& o : obstacles)
        {
            const float w = o.type == 0 ? 7.0f : 12.0f, h = o.type == 0 ? 7.0f : 7.0f;
            if (owlR > o.x + 1.0f && owlL < o.x + w - 1.0f && owlBottom < h - 1.0f)
            {
                state = State::Over;
                hi = juce::jmax (hi, (int) score);
                setDescription ("Try again. Score " + juce::String ((int) score) + ", best " + juce::String (hi) + ".");
                break;
            }
        }
    }
    repaint();
}

void GameView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::surface);
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (Colours::outline);
    g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);

    const int u = kUnit;
    const juce::Point<int> o ((getWidth() - kGridW * u) / 2, (getHeight() - kGridH * u) / 2);
    auto px = [&] (float x, float y, float w, float h, juce::Colour c)
    {
        g.setColour (c);
        g.fillRect (o.x + juce::roundToInt (x) * u, o.y + juce::roundToInt (y) * u, juce::roundToInt (w) * u, juce::roundToInt (h) * u);
    };

    // луна из пикселей
    const int mx = kGridW - 74, my = 8;
    static const char* moon[7] = { "..xxx..", ".xxxxx.", "xxxxxxx", "xxxxxxx", "xxxxxxx", ".xxxxx.", "..xxx.." };
    for (int y = 0; y < 7; ++y)
        for (int x = 0; x < 7; ++x)
            if (moon[y][x] == 'x') px ((float) (mx + x), (float) (my + y), 1, 1, Colours::track);

    // редкие звёзды
    static const int stars[][2] = { { 70, 6 }, { 112, 18 }, { 160, 14 }, { 186, 26 }, { 38, 22 }, { 140, 4 } };
    for (auto& s : stars) px ((float) s[0], (float) s[1], 1, 1, Colours::ink2.withAlpha (0.5f));

    // земля и камешки
    px (4, (float) kGround, (float) kGridW - 8, 1, Colours::ink2);
    static const int pebbles[][2] = { { 10, 2 }, { 37, 1 }, { 71, 2 }, { 104, 1 }, { 139, 2 }, { 171, 1 }, { 203, 2 } };
    const int shift = (int) pebbleOffset % kGridW;
    for (auto& p : pebbles)
    {
        int x = p[0] - shift;
        if (x < 4) x += kGridW - 8;
        if (x < kGridW - 6) px ((float) x, (float) kGround + 2.0f + (p[1] == 2 ? 1.0f : 0.0f), (float) p[1], 1, Colours::ink2.withAlpha (0.6f));
    }

    // препятствия: пень и стог сена
    static const char* stump[7] = { ".xxxxx.", "xxxxxxx", "xxxxxxx", "xxxxxxx", "xxxxxxx", "xxxxxxx", "xxxxxxx" };
    static const char* hay[7] = { "...xxxxxx...", ".xxxxxxxxxx.", "xxxxxxxxxxxx", "xxxxxxxxxxxx", "xxxxxxxxxxxx", "xxxxxxxxxxxx", "xxxxxxxxxxxx" };
    for (auto& ob : obstacles)
    {
        const char** spr = ob.type == 0 ? stump : hay;
        const int w = ob.type == 0 ? 7 : 12;
        for (int y = 0; y < 7; ++y)
            for (int x = 0; x < w; ++x)
                if (spr[y][x] == 'x' && ob.x + (float) x >= 4.0f && ob.x + (float) x < (float) kGridW - 4.0f)
                    px (ob.x + (float) x, (float) (kGround - 7 + y), 1, 1, Colours::ink2);
    }

    // тень под совой (сжимается в прыжке)
    const int shadowW = juce::jmax (2, 8 - (int) (owlY / 5.0f));
    px ((float) (kOwlX + 6 - shadowW / 2), (float) kGround + 1.0f, (float) shadowW, 1, Colours::track);

    // сова
    int f = 0;
    if (state == State::Over) f = 3;
    else if (owlY > 0.5f) f = 2;
    else if (state == State::Running) f = (frame / 6) % 2;
    const int owlTop = kGround - 13 - juce::roundToInt (owlY);
    drawOwl (g, { o.x + kOwlX * u, o.y + owlTop * u }, u, f);

    // счёт: рекорд слева, текущий справа
    g.setFont (LnF::mono (LnF::Type::subheadline, false));
    auto scoreArea = juce::Rectangle<int> (o.x, o.y + 4, kGridW * u - 12, 16);
    g.setColour (Colours::ink);
    g.setFont (LnF::mono (LnF::Type::subheadline, true));
    const auto cur = juce::String ((int) score).paddedLeft ('0', 5);
    g.drawText (cur, scoreArea.removeFromRight (48), juce::Justification::centredRight);
    g.setFont (LnF::mono (LnF::Type::subheadline, false));
    g.setColour (Colours::ink2);
    g.drawText ("HI " + juce::String (hi).paddedLeft ('0', 5), scoreArea.removeFromRight (90), juce::Justification::centredRight);

    if (state == State::Over)
    {
        g.setColour (Colours::ink);
        g.setFont (LnF::font (LnF::Type::headline, true));
        g.drawText ("Try again", getLocalBounds().withTrimmedBottom (40), juce::Justification::centred);
    }
    if (hasKeyboardFocus (false))
        LnF::drawFocusRing (g, r, 12.0f);
}

// ===========================================================================
void AboutPage::BackLogo::paintButton (juce::Graphics& g, bool over, bool)
{
    // маленький логотип — клик возвращает в плагин
    g.setColour (over ? Colours::ink.withAlpha (0.7f) : Colours::ink);
    g.setFont (LnF::pixel (26.0f));
    g.drawText ("Mist", getLocalBounds(), juce::Justification::centredLeft);
    if (hasKeyboardFocus (false)) LnF::drawFocusRing (g, getLocalBounds().toFloat(), 8.0f);
}

void AboutPage::CapsuleButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    auto fill = Colours::well;
    if (over) fill = fill.overlaidWith (Colours::ink.withAlpha (0.04f));
    if (down) fill = fill.overlaidWith (Colours::ink.withAlpha (0.08f));
    g.setColour (fill);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (Colours::outline);
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
    g.setColour (Colours::ink);
    g.setFont (LnF::font (LnF::Type::body));
    g.drawText (getButtonText(), getLocalBounds(), juce::Justification::centred);
    if (hasKeyboardFocus (false)) LnF::drawFocusRing (g, r.expanded (3.0f), r.getHeight() * 0.5f + 3.0f);
}

void AboutPage::SecretLogo::mouseDown (const juce::MouseEvent&)
{
    // пасхалка: пять быстрых кликов по большому логотипу открывают игру
    const auto now = juce::Time::getMillisecondCounter();
    clicks = (now - lastClick < 450) ? clicks + 1 : 1;
    lastClick = now;
    if (clicks >= 5)
    {
        clicks = 0;
        if (onSecretClicks) onSecretClicks();
    }
}

AboutPage::AboutPage()
{
    setTitle ("About Mist");
    setDescription (juce::String ("Mist ") + AboutInfo::version + ", " + AboutInfo::date + ", " + AboutInfo::author);
    addAndMakeVisible (back);
    addAndMakeVisible (logoHit);
    logoHit.setTitle ("Mist logo");
    logoHit.onSecretClicks = [this] { showGame (true); };
    addChildComponent (closeGame);
    addChildComponent (game);
    back.onClick = [this] { showGame (false); if (onClose) onClose(); };
    closeGame.onClick = [this] { showGame (false); };
}

void AboutPage::showGame (bool show)
{
    gameMode = show;
    logoHit.setVisible (! show);
    game.setVisible (show);
    closeGame.setVisible (show);
    if (show) game.start(); else game.stop();
    resized();
    repaint();
}

void AboutPage::drawVersion (juce::Graphics& g, juce::Rectangle<float> r, bool centred)
{
    const auto bold = LnF::font (LnF::Type::callout, true), regular = LnF::font (LnF::Type::callout);
    const juce::String v (AboutInfo::version), sep (midDot()), d (AboutInfo::date);
    const float wv = (float) juce::GlyphArrangement::getStringWidth (bold, v);
    const float ws = (float) juce::GlyphArrangement::getStringWidth (regular, sep);
    const float wd = (float) juce::GlyphArrangement::getStringWidth (regular, d);
    float x = centred ? r.getCentreX() - (wv + ws + wd) * 0.5f : r.getX();
    g.setColour (Colours::ink);  g.setFont (bold);    g.drawText (v, juce::Rectangle<float> (x, r.getY(), wv + 2, r.getHeight()), juce::Justification::centredLeft); x += wv;
    g.setColour (Colours::ink2); g.setFont (regular); g.drawText (sep, juce::Rectangle<float> (x, r.getY(), ws + 2, r.getHeight()), juce::Justification::centredLeft); x += ws;
    g.drawText (d, juce::Rectangle<float> (x, r.getY(), wd + 2, r.getHeight()), juce::Justification::centredLeft);
}

void AboutPage::paint (juce::Graphics& g)
{
    g.fillAll (Colours::window);
    // едва заметная точечная сетка
    g.setColour (Colours::ink.withAlpha (0.07f));
    for (int y = 12; y < getHeight(); y += 16)
        for (int x = 12; x < getWidth(); x += 16)
            g.fillRect (x, y, 1, 1);

    // большое «Mist»
    g.setColour (Colours::ink);
    const float targetW = gameMode ? 150.0f : 320.0f;
    auto f = LnF::pixel (100.0f);
    const float w100 = (float) juce::GlyphArrangement::getStringWidth (f, "Mist");
    f = LnF::pixel (100.0f * targetW / juce::jmax (1.0f, w100));
    g.setFont (f);
    g.drawText ("Mist", bigLogo, juce::Justification::centred);

    // под логотипом — автор
    g.setColour (Colours::ink2);
    g.setFont (LnF::font (LnF::Type::callout));
    g.drawText (AboutInfo::author, versionLine, juce::Justification::centred);

    // версия и дата — в левом нижнем углу
    drawVersion (g, juce::Rectangle<float> (28.0f, (float) getHeight() - 42.0f, 400.0f, 18.0f), false);

    if (gameMode)
    {
        g.setColour (Colours::ink2);
        g.setFont (LnF::font (LnF::Type::footnote));
        g.drawText (juce::String::fromUTF8 ("Space, \xe2\x86\x91 or tap to jump"), hintLine, juce::Justification::centred);
    }
}

void AboutPage::resized()
{
    const float W = (float) getWidth(), H = (float) getHeight();
    back.setBounds (24, 16, 84, 40);

    if (! gameMode)
    {
        bigLogo = { 0.0f, H * 0.33f, W, 140.0f };
        versionLine = { 0.0f, bigLogo.getBottom() + 4.0f, W, 18.0f };
        logoHit.setBounds (bigLogo.withSizeKeepingCentre (360.0f, bigLogo.getHeight()).toNearestInt());
    }
    else
    {
        bigLogo = { 0.0f, H * 0.12f, W, 80.0f };
        versionLine = { 0.0f, bigLogo.getBottom() + 2.0f, W, 18.0f };
        game.setBounds ((int) (W - 680.0f) / 2, (int) versionLine.getBottom() + 34, 680, 220);
        hintLine = { 0.0f, (float) game.getBottom() + 14.0f, W, 16.0f };
        closeGame.setBounds ((int) (W * 0.5f) - 65, (int) hintLine.getBottom() + 16, 130, 40);
    }
}
} // namespace mist
