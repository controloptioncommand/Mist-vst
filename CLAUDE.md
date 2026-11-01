# Правила проекта Mist

Mist — вайбкод VST3/AU-синтезатор (sample-based FM) для macOS, C++ / JUCE 8.
Репозиторий публичный: https://github.com/controloptioncommand/Mist-vst

Перед любой работой с интерфейсом прочитай **[docs/DESIGN.md](docs/DESIGN.md)** — это обязательная дизайн-система.

## Общение
- Отвечать по-русски, коротко и по делу. Не начинать ответ с похвалы.
- Менять мнение только на аргументах, а не потому что пользователь не согласен.
- Если просьба неоднозначна (особенно по дизайну) — уточнить, а не придумывать от себя.

## Приватность (репозиторий открытый)
- Никаких личных данных в файлах, коммитах и патчноутах: почт, паролей, токенов, API-ключей, локальных путей, ссылок на переписку с ИИ (claude.ai/…).
- Коммиты: автор `nnn <controloptioncommand@users.noreply.github.com>`, коммиттер `Claude <noreply@anthropic.com>`. Строку `Claude-Session: …` в сообщения коммитов **не добавлять**.
  ```bash
  GIT_COMMITTER_NAME=Claude GIT_COMMITTER_EMAIL=noreply@anthropic.com \
    git -c user.name="nnn" -c user.email="controloptioncommand@users.noreply.github.com" commit -m "…"
  ```
- Перед каждым push: `bash scripts/privacy-check.sh` (CI тоже его запускает и остановит сборку при утечке).
- Не делать force push и не переписывать опубликованную историю без явной просьбы.
- Публично оставлены намеренно: ник «nnn» и ссылка на Telegram автора на странице About.

## Версии и патчноуты
- Текущая версия — в файле `VERSION` (схема: v1, v1.01 … v2, v2.01 …). На странице About она же в `Source/UI/About.h` (`AboutInfo::version`, `AboutInfo::date`).
- **У каждой версии обязателен патчноут** `docs/releases/<версия>.md` — без него CI не соберёт релиз.
- Новая версия: поменять `VERSION` и `AboutInfo` → добавить патчноут → коммит в `main`. CI сам соберёт, проверит (pluginval, auval, рендер-тест) и опубликует релиз с архивом `Mist-macOS.zip`.
- Пока `VERSION` не меняется, новые сборки перезаписывают текущий релиз.
- Ссылка на последнюю сборку: https://github.com/controloptioncommand/Mist-vst/releases/latest/download/Mist-macOS.zip
- После публикации дать пользователю эту ссылку и команду установки:
  ```bash
  cd ~/Downloads && rm -rf Mist-macOS Mist-macOS.zip && gh release download -R controloptioncommand/Mist-vst -p Mist-macOS.zip && unzip -q Mist-macOS.zip -d Mist-macOS && bash Mist-macOS/install-mac.sh
  ```

## Сборка и проверка
```bash
cmake -B build -DMIST_BUILD_TESTS=ON && cmake --build build --target MistRenderTest
./build/MistRenderTest_artefacts/Release/MistRenderTest ./render   # на Linux — через xvfb-run -a
```
Рендер-тест: пресеты, 32 алгоритма в худшем случае, семплы, Grain, оверсемплинг, 40 бросков кубика (не тишина, не NaN, ≤ 0 dBFS), сохранение состояния, скриншоты интерфейса в обеих темах. Перед коммитом тест должен печатать `ALL OK`. После изменений интерфейса — посмотреть скриншоты.

## Звук: что нельзя ломать
- Кубик (`Source/Randomizer.cpp`) не трогает Master, семплы и их start/loop/reverse/root, режимы операторов, Drone, оверсемплинг, интерполяцию, полифонию, Mod Matrix. Несущие ≥ 65%, срез фильтра ≥ 900 Гц, резонанс ≤ 50%.
- Fine — двуполярный (−50…+50%). Изменение диапазонов параметров ломает старые пресеты — предупреждать.
- Выход защищён мягким лимитером (не выше 0 dBFS).

## Структура
- `Source/DSP/` — синтез (голоса, операторы, зёрна, LFO, wavetable, детектор высоты, огибающие, фильтр)
- `Source/Algorithms.cpp` — 32 алгоритма; `Source/Randomizer.cpp` — кубик
- `Source/UI/` — интерфейс: `LookAndFeel` (темы, шрифты Medeo, плашки, ручки), `Visualizer`, `OperatorPanel`, `RoutingPanel`, `ModulationPanel`, `About` (страница About, «Туман», игра)
- `Tests/RenderTest.cpp` — офлайн-тест; `scripts/` — установка и проверка приватности; `docs/` — картинки README, патчноуты, дизайн
