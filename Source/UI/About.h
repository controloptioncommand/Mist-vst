#pragma once
#include "LookAndFeel.h"

namespace mist
{
// Версия и дата — показываются на странице About
namespace AboutInfo
{
    inline const char* version = "v2";
    inline const char* repoUrl = "https://github.com/controloptioncommand/Mist-vst";
    inline const char* authorUrl = "https://t.me/nothiingnothingnothing";
    inline const char* date = "1st November 2026";
    inline const char* author = "made by nnn";
}

// Пиксельная сова-сипуха 12×13 (тело и крылья — сигнальный цвет, лицо-сердечко и грудка светлые)
void drawOwl (juce::Graphics& g, juce::Point<int> origin, int unit, int frame);   // frame: 0,1 бег, 2 прыжок, 3 сидит

// Бегалка: сова прыгает через пни и стог сена
class GameView : public juce::Component, private juce::Timer
{
public:
    GameView();
    void start();
    void stop();
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent&) override { jump(); }
    bool keyPressed (const juce::KeyPress& k) override;
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { repaint(); }

private:
    void timerCallback() override;
    void jump();
    void reset();
    void spawnObstacle();

    enum class State { Ready, Running, Over };
    struct Obstacle { float x; int type; };   // 0 — пень, 1 — стог сена

    static constexpr int kUnit = 3, kGridW = 226, kGridH = 73, kGround = 60, kOwlX = 22;
    State state = State::Ready;
    float owlY = 0.0f, velY = 0.0f, speed = 1.6f, score = 0.0f, nextGap = 60.0f, sinceLast = 0.0f, pebbleOffset = 0.0f;
    int hi = 0, frame = 0;
    std::vector<Obstacle> obstacles;
    juce::Random rng;
};

// Страница About: на всё окно. Логотип, версия/дата, Play → игра.
class AboutPage : public juce::Component
{
public:
    AboutPage();
    void paint (juce::Graphics& g) override;
    void resized() override;
    void showGame (bool show);
    std::function<void()> onClose;

private:
    struct BackLogo : juce::Button
    {
        BackLogo() : juce::Button ("Mist") { setTitle ("Back to Mist"); }
        void paintButton (juce::Graphics&, bool over, bool down) override;
    };
    struct SecretLogo : juce::Component
    {
        std::function<void()> onSecretClicks;
        void mouseDown (const juce::MouseEvent&) override;
        juce::uint32 lastClick = 0;
        int clicks = 0;
    };
    // Невидимая кликабельная область поверх текста: внешне ничем не выделена
    struct HiddenLink : juce::Component
    {
        HiddenLink (const juce::String& title, const juce::String& u) : url (u)
        {
            setTitle (title);
            setDescription (u);
            setWantsKeyboardFocus (true);
            setMouseClickGrabsKeyboardFocus (false);
        }
        void mouseUp (const juce::MouseEvent& e) override { if (e.mouseWasClicked()) open(); }
        bool keyPressed (const juce::KeyPress& k) override
        {
            if (k == juce::KeyPress::returnKey || k == juce::KeyPress::spaceKey) { open(); return true; }
            return false;
        }
        void paint (juce::Graphics& g) override
        {
            if (hasKeyboardFocus (false)) MistLookAndFeel::drawFocusRing (g, getLocalBounds().toFloat(), 6.0f);
        }
        void focusGained (FocusChangeType) override { repaint(); }
        void focusLost (FocusChangeType) override { repaint(); }
        void open() { juce::URL (url).launchInDefaultBrowser(); }
        juce::String url;
    };
    struct CapsuleButton : juce::Button
    {
        explicit CapsuleButton (const juce::String& t) : juce::Button (t) { setTitle (t); }
        void paintButton (juce::Graphics&, bool over, bool down) override;
    };

    void drawVersion (juce::Graphics& g, juce::Rectangle<float> r, bool centred = true);

    BackLogo back;
    SecretLogo logoHit;
    HiddenLink versionLink { "Mist on GitHub", AboutInfo::repoUrl };
    HiddenLink authorLink { "Author on Telegram", AboutInfo::authorUrl };
    CapsuleButton closeGame { "Close game" };
    GameView game;
    bool gameMode = false;
    juce::Rectangle<float> bigLogo, versionLine, hintLine;
};
} // namespace mist
