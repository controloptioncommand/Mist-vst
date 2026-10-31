# Loom — sample-based FM synth

Личный VST3/AU-инструмент: 6 операторов, фазовая модуляция, где источником каждого оператора может быть семпл.

## Возможности (v0.1)

- **6 операторов**, у каждого режим:
  - **Wave** — band-limited wavetable. Без семпла это встроенная таблица (Sine → Sine+Oct → Tri → Square → Saw → Pulse → Narrow → Formant). С семплом таблица из 64 кадров нарезается по найденной высоте тона. **Scan** двигает кадр.
  - **Sample** — воспроизведение семпла, фазовая модуляция сдвигает позицию чтения. Есть start, петля (Forward / Ping-Pong) с кроссфейдом, reverse, root и keytrack.
- **32 алгоритма** и режим **Matrix**: любой оператор модулирует любой (36 ячеек), выход каждого оператора в микс регулируется отдельно. Кнопка «Edit as Matrix» переносит текущий алгоритм в матрицу.
- Feedback на каждом операторе. Ratio / Fine / Detune / Fixed Hz.
- Огибающая DAHDSR на каждом операторе, времена до 30 с.
- 8 голосов полифонии, режимы Mono и Legato, glide, unison до 8 голосов с detune и стерео-разбросом.
- Фильтр: LP12, LP24, BP, HP, Notch, с drive, keytrack и модуляционной огибающей.
- 2 LFO: 7 форм, синхронизация с темпом и транспортом.
- Mod Matrix на 8 слотов. Источники: LFO, Mod Env, Velocity, Key, Mod Wheel, Aftertouch, Random. Назначения: глобальные параметры, а также Level / Pitch / Scan / Feedback каждого оператора.
- Пресеты (`~/Music/Loom/Presets/*.loom`) хранят семплы внутри файла (FLAC). Проект DAW тоже хранит семплы внутри.
- Окно масштабируется от 50% до 200%.

## Установка на macOS

1. Откройте вкладку **Actions** репозитория, выберите последний успешный запуск **Build macOS** и скачайте артефакт `Loom-macOS`.
2. Распакуйте архив и выполните в Терминале:
   ```bash
   bash ~/Downloads/Loom-macOS/install-mac.sh
   ```
3. В Ableton Live: **Settings → Plug-Ins → Rescan**.

## Сборка вручную

```bash
cmake -B build -G Xcode
cmake --build build --config Release --target Loom_VST3 Loom_AU
```

Офлайн-тест (рендер пресетов в WAV, проверка NaN, перегрузки и скорости):

```bash
cmake -B build -DLOOM_BUILD_TESTS=ON && cmake --build build --target LoomRenderTest
./build/LoomRenderTest_artefacts/Release/LoomRenderTest ./render
```

## Структура

- `Source/DSP/` — синтез: `Synth` (голоса, операторы, LFO), `Wavetable`, `SampleAsset` (загрузка, детектор высоты тона, нарезка таблиц), `Primitives` (огибающая, SVF-фильтр).
- `Source/Algorithms.cpp` — таблица 32 алгоритмов.
- `Source/Params.cpp` — все параметры.
- `Source/UI/` — интерфейс. Цвета собраны в `LookAndFeel.h`, интерфейс рисуется в базовом размере 1280×820 и масштабируется.
