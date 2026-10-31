#!/usr/bin/env python3
"""Генерирует HTML руководства Loom (затем печатается в PDF через Chromium)."""
import re, base64, pathlib, html

HERE = pathlib.Path(__file__).parent
SRC = (pathlib.Path(__file__).resolve().parents[2] / "Source" / "Algorithms.cpp").read_text()
OPC = ["#5b5bd6", "#e5484d", "#f76b15", "#30a46c", "#0ea5b7", "#d6409f"]

algos = re.findall(r'\{\s*"([^"]+)",\s*"([^"]*)",\s*"([^"]+)"\s*\}', SRC)
assert len(algos) == 32, len(algos)


def img(name):
    return "data:image/png;base64," + base64.b64encode((HERE / name).read_bytes()).decode()


def algo_svg(edges_s, carriers_s, w=150, h=150):
    edges = [(int(e[0]) - 1, int(e[1]) - 1) for e in edges_s.split()]
    out = [int(c) - 1 for c in carriers_s]
    lvl = [0 if i in out else -1 for i in range(6)]
    # самый длинный путь до выхода (несущие фиксированы на 0, петли ограничены)
    for _ in range(8):
        for s, d in edges:
            if s in out or lvl[d] < 0:
                continue
            lvl[s] = min(5, max(lvl[s], lvl[d] + 1))
    lvl = [max(0, l) for l in lvl]
    mx = max(lvl)
    pos = {}
    for L in range(mx + 1):
        row = [i for i in range(6) if lvl[i] == L]
        if L > 0:
            def avg(s):
                xs = [pos[d][0] for (a, d) in edges if a == s and d in pos]
                return sum(xs) / len(xs) if xs else 1e6 + s
            row.sort(key=avg)
        y = h - 28 if mx == 0 else (h - 28) - L * (h - 46) / mx
        if mx == 0:
            y = h / 2 - 6
        for k, i in enumerate(row):
            pos[i] = (14 + (w - 28) * (k + 0.5) / len(row), y)
    r = 7.5
    parts = [f'<svg viewBox="0 0 {w} {h}" width="{w}" height="{h}" xmlns="http://www.w3.org/2000/svg">',
             '<defs><marker id="a" viewBox="0 0 6 6" refX="5" refY="3" markerWidth="5" markerHeight="5" orient="auto">'
             '<path d="M0,0 L6,3 L0,6 z" fill="#8e8e93"/></marker></defs>']
    for s, d in edges:
        (x1, y1), (x2, y2) = pos[s], pos[d]
        if lvl[s] - lvl[d] > 1 and abs(x1 - x2) < 6:
            # связь «через уровень» на одной вертикали — дугой сбоку, чтобы не пряталась за кружками
            bow = 8 + 7 * (lvl[s] - lvl[d])
            parts.append(f'<path d="M{x1 + r * .7:.1f},{y1 + r * .5:.1f} Q{x1 + bow:.1f},{(y1 + y2) / 2:.1f} {x2 + r * .7:.1f},{y2 - r * .6:.1f}" '
                         f'fill="none" stroke="#8e8e93" stroke-width="1.1" marker-end="url(#a)"/>')
        elif lvl[s] > lvl[d]:
            parts.append(f'<line x1="{x1:.1f}" y1="{y1 + r:.1f}" x2="{x2:.1f}" y2="{y2 - r - 1:.1f}" stroke="#8e8e93" stroke-width="1.3" marker-end="url(#a)"/>')
        else:
            side = -1 if x1 <= x2 else 1
            bend = 22 + 4 * abs(s - d)
            parts.append(f'<path d="M{x1 + side * r * .7:.1f},{y1:.1f} C{x1 + side * bend:.1f},{y1 - 14:.1f} {x2 + side * bend:.1f},{y2 - 14:.1f} {x2 + side * r * .7:.1f},{y2 - r * .7:.1f}" '
                         f'fill="none" stroke="#c0c0c6" stroke-width="1.2" stroke-dasharray="3 2" marker-end="url(#a)"/>')
    bus = h - 10
    xs = [pos[i][0] for i in out]
    for i in out:
        x, y = pos[i]
        parts.append(f'<line x1="{x:.1f}" y1="{y + r:.1f}" x2="{x:.1f}" y2="{bus}" stroke="#c7c7cc" stroke-width="1.2"/>')
    parts.append(f'<line x1="{min(xs) - 4:.1f}" y1="{bus}" x2="{max(xs) + 4:.1f}" y2="{bus}" stroke="#8e8e93" stroke-width="2"/>')
    for i in range(6):
        x, y = pos[i]
        c = OPC[i]
        fill = c if i in out else "#fff"
        tc = "#fff" if i in out else c
        parts.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r}" fill="{fill}" stroke="{c}" stroke-width="1.5"/>')
        parts.append(f'<text x="{x:.1f}" y="{y + 3.0:.1f}" font-size="8.5" font-weight="700" text-anchor="middle" fill="{tc}">{i + 1}</text>')
    parts.append("</svg>")
    return "".join(parts)


algo_cards = "".join(
    f'<div class="algo"><div class="an"><b>{n + 1}</b> {html.escape(name)}</div>{algo_svg(e, c)}</div>'
    for n, (name, e, c) in enumerate(algos))


def table(rows, head=("Параметр", "Диапазон", "Что делает")):
    h = "".join(f"<th>{x}</th>" for x in head)
    b = "".join("<tr>" + "".join(f"<td>{x}</td>" for x in r) + "</tr>" for r in rows)
    return f'<table><thead><tr>{h}</tr></thead><tbody>{b}</tbody></table>'


op_common = table([
    ("On", "вкл / выкл", "Точка у названия слота. Выключенный оператор не звучит и не модулирует."),
    ("Depth", "0–100%", "Уровень оператора. Для несущей — громкость, для модулятора — глубина модуляции. Кривая квадратичная: малые значения настраиваются точно."),
    ("Ratio", "0.5, 1–32", "Отношение частоты оператора к частоте ноты."),
    ("Fine", "0–100%", "Дробная добавка к Ratio: итог = Ratio × (1 + Fine). Нецелые отношения дают металлические, колокольные спектры."),
    ("Detune", "±100 ct", "Расстройка в центах. Пара центов на модуляторе даёт медленное «биение» тембра."),
    ("Feedback", "0–100%", "Оператор модулирует сам себя. Синус превращается в пилу, затем в шум."),
    ("Velocity", "0–100%", "Насколько сила нажатия влияет на уровень этого оператора."),
    ("Pan", "L–R", "Панорама (для несущих)."),
    ("Fixed freq / Fixed Hz", "0.1 Гц – 10 кГц", "Частота не зависит от ноты. Удобно для формант, «гула» и низкочастотной модуляции."),
    ("Matrix Out", "0–100%", "Только в режиме Matrix: сколько оператора идёт на выход."),
])

op_sample = table([
    ("Start", "0–100%", "Откуда начинается воспроизведение."),
    ("Loop In / Loop Out", "0–100%", "Границы петли. Их можно тянуть прямо на картинке волны."),
    ("X-Fade", "0–50%", "Кроссфейд на стыке петли (режим Forward), убирает щелчок."),
    ("Loop", "Off / Forward / Ping-Pong", "Без петли, по кругу, туда-обратно."),
    ("Reverse", "вкл / выкл", "Играть задом наперёд (кнопка в слоте)."),
    ("Root", "C-1 – G9", "Нота, на которой семпл звучит в оригинальной высоте. Определяется автоматически при загрузке."),
    ("Keytrack", "вкл / выкл", "Выкл — семпл всегда звучит на Root, клавиши не меняют высоту (полезно для текстур)."),
])

op_grain = table([
    ("Position", "0–100%", "Место в семпле, откуда берутся зёрна. Тянется по картинке волны."),
    ("Size", "10 мс – 1 с", "Длина одного зерна. Короткие — «шершавый» шум, длинные — плавные облака."),
    ("Density", "x1 – x6", "Сколько зёрен перекрываются одновременно."),
    ("Spray", "0–100%", "Случайный разброс позиции и момента старта зёрен."),
    ("Root, Keytrack, Reverse", "", "Как в Sample PM."),
])

env_t = table([
    ("Delay", "0–20 с", "Пауза перед атакой."),
    ("Attack", "0–30 с", "Нарастание."),
    ("Hold", "0–20 с", "Удержание на максимуме."),
    ("Decay", "0–30 с", "Спад до уровня Sustain."),
    ("Sustain", "0–100%", "Уровень, пока нажата клавиша."),
    ("Release", "0–30 с", "Затухание после отпускания."),
])

voice_t = table([
    ("Voice mode", "Poly / Mono / Legato", "Poly — до 8 нот. Mono — одна нота, каждая перезапускает огибающие. Legato — одна нота, при связной игре огибающие не перезапускаются."),
    ("Glide", "0–5 с", "Скольжение высоты между нотами."),
    ("Bend", "0–24 полутона", "Диапазон питч-бенда."),
    ("FM Amount", "0–400%", "Общая глубина всей FM-модуляции сразу. Главная ручка «яркости/грязи»."),
    ("Master", "−48 … +6 дБ", "Общая громкость."),
    ("Unison", "1–8", "Копии каждой ноты, слегка расстроенные между собой."),
    ("Detune", "0–100 ct", "Разброс расстройки унисона."),
    ("Spread", "0–100%", "Ширина стерео для голосов унисона."),
])

filter_t = table([
    ("On / Type", "LP 12, LP 24, BP, HP, Notch", "Фильтр на каждом голосе, стерео."),
    ("Cutoff", "20 Гц – 20 кГц", "Частота среза."),
    ("Reso", "0–100%", "Резонанс."),
    ("Env Amt", "±100%", "Сколько Mod Env двигает срез (±6 октав)."),
    ("Key", "0–100%", "Срез следует за нотой."),
    ("Drive", "0–100%", "Мягкий перегруз перед фильтром."),
    ("Mod Env A/D/S/R", "до 30 с", "Отдельная огибающая для фильтра и Mod Matrix."),
])

mod_t = table([
    ("LFO 1, LFO 2", "−1…+1", "Глобальные LFO."),
    ("Mod Env", "0…1", "Огибающая модуляции (вкладка Filter)."),
    ("Velocity", "0…1", "Сила нажатия."),
    ("Key", "−1…+1", "Высота ноты относительно C3."),
    ("Mod Wheel", "0…1", "Колесо модуляции (CC1)."),
    ("Aftertouch", "0…1", "Давление на клавишу."),
    ("Random", "−1…+1", "Случайное число, своё на каждую ноту."),
], head=("Источник", "Значения", "Описание"))

dst_t = table([
    ("Pitch", "±24 полутона", "Высота всего голоса."),
    ("FM Amount", "±200%", "Общая глубина FM."),
    ("Cutoff / Resonance", "±6 октав / ±100%", "Фильтр."),
    ("Uni Detune", "×0…×3", "Расстройка унисона."),
    ("Volume / Pan", "", "Громкость и панорама голоса."),
    ("Op N Level", "±100%", "Depth конкретного оператора."),
    ("Op N Pitch", "±24 полутона", "Высота конкретного оператора."),
    ("Op N Scan", "±100%", "Position оператора (Scrub, Grain)."),
    ("Op N Feedback", "±100%", "Самомодуляция оператора."),
], head=("Назначение", "При Amount = 100%", "Что двигает"))

factory = table([
    ("Init FM", "Простейшая пара 2→1. Отправная точка."),
    ("Glass Pad", "Три пары, медленная атака, унисон 4, LFO на FM Amount."),
    ("Dark Ring Drone", "Алгоритм Ring 6, унисон 6, случайный LFO на FM и Position."),
    ("Metal Bell", "Пять модуляторов в один, нецелые отношения, длинный спад."),
    ("Breathing Swarm", "Cross Pairs, унисон 8, «дышащая» расстройка."),
    ("Formant Choir", "Формантная волна, BP-фильтр, вибрато, колесо двигает Position."),
    ("Chaos Engine", "Алгоритм Chaos, моно с глайдом, S&H по темпу."),
    ("Hollow Sub", "Плотный бас, Legato."),
], head=("Пресет", "Что это"))

gestures = table([
    ("Ручка", "Тянуть вверх/вниз. Двойной клик — значение по умолчанию."),
    ("Слот семпла", "Перетащить аудиофайл — загрузить. Двойной клик по волне — выбрать файл. Правый клик — меню (загрузить, очистить, режим). Клик по точке — вкл/выкл оператор."),
    ("Картинка волны (вкладка Operator)", "Тянуть маркеры Start / Loop In / Loop Out; в Grain и Scrub — Position."),
    ("Матрица (Routing)", "Тянуть клетку вверх/вниз — глубина. Shift — точнее. Двойной клик — вкл/выкл. Диагональ «fb» — Feedback."),
    ("Схема алгоритма", "Клик по кружку — выбрать оператор."),
    ("Визуализация", "Waves / Spectrum / Scope — переключение вида. «Drone on» — вкл/выкл Drone."),
    ("Иконка настроек", "Сохранить пресет, Init, папка пресетов, удалить пресет, размер окна 75–150%."),
], head=("Где", "Как"))

trouble = table([
    ("Loom нет в Ableton", "Settings → Plug-Ins: включить Use Audio Units и/или VST3 System Folders, нажать Rescan. Если не помогло — ещё раз запустить install-mac.sh."),
    ("macOS пишет, что плагин повреждён", "Плагин без подписи Apple. install-mac.sh снимает карантин; если ставили вручную — выполнить скрипт."),
    ("Нет звука", "Проверить: несущие (закрашенные кружки на схеме) включены и Depth > 0; Master; не выключен ли оператор точкой в слоте."),
    ("Ноты не замолкают", "Включён Drone — выключить кнопкой внизу или «Drone on» в визуализации."),
    ("Высокая нагрузка CPU", "Oversampling 1x, меньше Unison, меньше операторов в режиме Grain."),
    ("Семпл звучит не в той тональности", "Поправить Root на вкладке Operator (автоопределение могло ошибиться на шумном материале)."),
    ("Перегруз", "Убавить Master или FM Amount. На выходе стоит мягкий ограничитель, он не даёт превысить 0 дБ, но окрашивает звук."),
], head=("Проблема", "Решение"))

HTML = f"""<!doctype html><html lang="ru"><head><meta charset="utf-8"><title>Loom — руководство</title>
<style>
@page {{ size: A4; margin: 16mm 15mm 16mm 15mm; }}
* {{ box-sizing: border-box; }}
body {{ font-family: Inter, sans-serif; color: #1c1c1e; font-size: 10pt; line-height: 1.5; margin: 0; }}
h1 {{ font-size: 30pt; margin: 0 0 4mm; letter-spacing: -0.5pt; }}
h2 {{ font-size: 16pt; margin: 9mm 0 3mm; padding-top: 2mm; border-top: 1px solid #e3e3e8; break-after: avoid; }}
h3 {{ font-size: 11.5pt; margin: 5mm 0 2mm; break-after: avoid; }}
p {{ margin: 0 0 2.5mm; }}
.lead {{ font-size: 12pt; color: #3a3a3c; }}
.dim {{ color: #8e8e93; }}
.cover {{ height: 255mm; display: flex; flex-direction: column; justify-content: space-between; break-after: page; }}
.cover img {{ width: 100%; border-radius: 6px; box-shadow: 0 2px 12px rgba(0,0,0,.12); }}
.shot {{ width: 100%; border: 1px solid #e3e3e8; border-radius: 6px; margin: 2mm 0 3mm; }}
.two {{ display: grid; grid-template-columns: 1fr 1fr; gap: 4mm; }}
.two img {{ width: 100%; border: 1px solid #e3e3e8; border-radius: 6px; }}
table {{ width: 100%; border-collapse: collapse; margin: 2mm 0 4mm; font-size: 9pt; break-inside: auto; }}
th {{ text-align: left; font-weight: 600; color: #8e8e93; border-bottom: 1px solid #d1d1d6; padding: 1.5mm 2mm; }}
td {{ border-bottom: 1px solid #ececf0; padding: 1.6mm 2mm; vertical-align: top; }}
td:first-child {{ font-weight: 600; white-space: nowrap; }}
tr {{ break-inside: avoid; }}
.box {{ background: #f5f5f7; border-radius: 6px; padding: 3mm 4mm; margin: 3mm 0; break-inside: avoid; }}
.box b {{ color: #1e88ff; }}
code, .mono {{ font-family: 'DejaVu Sans Mono', monospace; font-size: 8.8pt; background: #f2f2f4; padding: .3mm 1.2mm; border-radius: 3px; }}
pre {{ font-family: 'DejaVu Sans Mono', monospace; font-size: 9pt; background: #1c1c1e; color: #f2f2f4; padding: 3mm 4mm; border-radius: 6px; white-space: pre-wrap; }}
ol, ul {{ margin: 0 0 3mm; padding-left: 6mm; }} li {{ margin-bottom: 1mm; }}
.algos {{ display: grid; grid-template-columns: repeat(4, 1fr); gap: 2.5mm; margin-top: 3mm; }}
.algo {{ border: 1px solid #e3e3e8; border-radius: 6px; padding: 1.5mm 2mm 0; break-inside: avoid; text-align: center; }}
.algo svg {{ width: 100%; height: auto; }}
.an {{ font-size: 8.5pt; text-align: left; }} .an b {{ color: #1e88ff; margin-right: 1mm; }}
.zones {{ display: grid; grid-template-columns: auto 1fr; gap: 1.5mm 4mm; font-size: 9.5pt; }}
.tag {{ display: inline-block; width: 6mm; height: 6mm; border-radius: 50%; color: #fff; font-weight: 700; text-align: center; line-height: 6mm; font-size: 8.5pt; background: #1e88ff; }}
.pb {{ break-before: page; }}
.toc li {{ margin-bottom: .6mm; }}
</style></head><body>

<section class="cover">
  <div>
    <div class="dim" style="font-size:10pt;letter-spacing:2pt">РУКОВОДСТВО · VST3 / AU · macOS</div>
    <h1 style="font-size:48pt;margin-top:6mm">Loom</h1>
    <p class="lead">Синтезатор на частотной (фазовой) модуляции, где источником каждого оператора может быть ваш семпл.
    Шесть операторов, 32 алгоритма и свободная матрица — для дронов, пэдов и саунд-дизайна.</p>
  </div>
  <img src="{img('ui_waves.png')}">
  <div class="dim">Версия 0.2 · пресеты: ~/Music/Loom/Presets</div>
</section>

<h2 style="border:0;margin-top:0">Содержание</h2>
<ol class="toc">
<li>Как это работает</li><li>Установка и обновление</li><li>Интерфейс</li><li>Операторы и режимы Sample PM / Scrub / Grain</li>
<li>Семплы</li><li>Маршрутизация: 32 алгоритма и матрица</li><li>Огибающие</li><li>Голос и унисон</li>
<li>Фильтр и Mod Env</li><li>LFO</li><li>Mod Matrix</li><li>Нижняя панель: Drone, интерполяция, оверсемплинг</li>
<li>Пресеты</li><li>Рецепты</li><li>Управление мышью</li><li>Если что-то не так</li>
</ol>

<h2>1. Как это работает</h2>
<p>В FM-синтезе есть <b>операторы</b> — простые генераторы. Одни операторы слышны напрямую — это <b>несущие</b> (carrier).
Другие не слышны, а «раскачивают» несущие, сдвигая их фазу с огромной скоростью, — это <b>модуляторы</b>. В результате
появляются новые обертоны: от тёплых до металлических и шумовых. Чем больше уровень модулятора (Depth) и общий
FM Amount, тем богаче и «грязнее» звук.</p>
<p>В классическом FM операторы — синусы. В Loom оператор может звучать как ваш семпл тремя способами:</p>
<ul>
<li><b>Sample PM</b> — семпл проигрывается, а модуляторы сдвигают точку чтения в нём, сэмпл за сэмплом. Живые, «грязные» текстуры.</li>
<li><b>Scrub</b> — из семпла нарезается 64-кадровая волновая таблица по его периоду. Ручка Position прокручивает по ней. Стабильная высота, без алиасинга — для пэдов и чистых тонов.</li>
<li><b>Grain</b> — короткие перекрывающиеся зёрна из выбранного места семпла; модуляторы сдвигают каждое зерно. Облака и шумные дроны.</li>
</ul>
<p>Какой оператор кого модулирует, задаёт <b>алгоритм</b> (32 готовых схемы) или <b>матрица</b> 6×6, где связи и их глубину можно задать вручную.</p>

<div style="break-inside:avoid"><h2>2. Установка и обновление</h2>
<ol>
<li>Скачайте последнюю сборку (нужен вход в GitHub):<br><span class="mono">github.com/controloptioncommand/loom/releases/latest/download/Loom-macOS.zip</span></li>
<li>Распакуйте архив двойным кликом — появится папка <b>Loom-macOS</b> в «Загрузках».</li>
<li>Откройте Терминал (Cmd + пробел → «Терминал») и выполните:</li>
</ol>
<pre>bash ~/Downloads/Loom-macOS/install-mac.sh</pre>
<p>Скрипт кладёт <b>Loom.vst3</b> и <b>Loom.component</b> в <span class="mono">~/Library/Audio/Plug-Ins</span> и снимает карантин macOS
(плагин не подписан сертификатом Apple). В Ableton: перезапуск, затем при необходимости Settings → Plug-Ins → Rescan.
Плагин работает на Apple Silicon, macOS 11 и новее.</p>
<div class="box"><b>Обновление.</b> Каждое изменение автоматически собирается и выкладывается по той же ссылке. Чтобы обновиться — повторите шаги 1–3.
Пресеты и проекты сохраняются.</div></div>

<h2 class="pb">3. Интерфейс</h2>
<img class="shot" src="{img('ui_waves.png')}">
<div class="zones">
<span class="tag">1</span><div><b>Шапка.</b> Название, браузер пресетов (‹ имя ›, клик по имени — список), переключатель режима выбранного оператора
<b>Sample PM / Scrub / Grain</b>, индикатор выхода <b>Out</b> (пик в дБ) и иконка настроек.</div>
<span class="tag">2</span><div><b>Визуализация результата</b> — что звучит на выходе прямо сейчас. Три вида: <b>Waves</b> (история волны «стопкой»,
свежая — синяя), <b>Spectrum</b> (спектрограмма и текущий спектр), <b>Scope</b> (форма волны, окрашенная по частотам: красный — низ,
зелёный — середина, синий — верх, и стереометр). Справа внизу — состояние Drone и самая низкая удерживаемая нота.</div>
<span class="tag">3</span><div><b>Слоты операторов.</b> Шесть слотов: цвет оператора, его роль (Carrier / Modulator / Unused / Off), имя семпла,
волна, Start и длина, кнопка Reverse. Сюда перетаскиваются семплы. Клик выбирает оператор.</div>
<span class="tag">4</span><div><b>Настройки</b> с вкладками: Operator, Envelope, Routing, Voice, Filter, LFO, Mod Matrix. Заголовок показывает
выбранный оператор и пояснение к режиму.</div>
<span class="tag">5</span><div><b>Нижняя панель:</b> Drone, Interpolation, Oversampling, число звучащих голосов и загрузка CPU.</div>
</div>
<div class="two" style="margin-top:4mm"><img src="{img('ui_spectrum.png')}"><img src="{img('ui_scope.png')}"></div>
<p class="dim" style="margin-top:1.5mm">Слева — вид Spectrum и вкладка Routing, справа — вид Scope и вкладка Mod Matrix.</p>

<h2>4. Операторы</h2>
<p>Выберите оператор кликом по слоту; его параметры — на вкладке <b>Operator</b>. Первая строка ручек общая для всех режимов, вторая зависит от режима.</p>
<h3>Общие параметры</h3>{op_common}
<h3>Sample PM</h3>
<p>Модуляторы сдвигают точку чтения семпла. Глубина модуляции 100% ≈ полтора периода основного тона семпла (по Root).</p>{op_sample}
<h3>Scrub</h3>
<p>Одна ручка <b>Position</b> — какой кадр волновой таблицы звучит (0% — начало семпла, 100% — конец). Высота ноты точная,
на высоких нотах нет алиасинга. Без загруженного семпла используется встроенная таблица:
Sine → Sine+Oct → Tri → Square → Saw → Pulse 25% → Pulse 10% → Formant.</p>
<h3>Grain</h3>{op_grain}

<h2>5. Семплы</h2>
<ul>
<li>Форматы: WAV, AIFF, FLAC, MP3, OGG, CAF, M4A. Стерео сводится в моно. Максимум 60 секунд.</li>
<li>При загрузке Loom ищет высоту тона (30 Гц – 2 кГц) и ставит <b>Root</b>. Если семпл шумовой и высота не найдена, Root остаётся C3.</li>
<li>Семпл нормализуется и сохраняется <b>внутри</b> пресета и проекта Ableton (FLAC). Исходный файл можно удалить.</li>
<li>Заменить семпл можно, просто перетащив новый на тот же слот, даже пока нота звучит.</li>
</ul>
<div class="box"><b>Совет.</b> Короткие тональные семплы (голос, струна, синт-нота) хорошо работают в Scrub. Длинные текстуры (поле,
шум плёнки, дождь) — в Sample PM и Grain. Отличная пара: тональный семпл — несущая, шумовой — модулятор.</div>

<h2 class="pb">6. Маршрутизация</h2>
<p>Вкладка <b>Routing</b>: выбор алгоритма, схема и матрица. На схеме закрашенные кружки — несущие (идут на выход, нижняя линия),
стрелки — кто кого модулирует. Пунктирные дуги — обратные связи и петли: они работают с задержкой в один сэмпл и дают
нестабильные, «живые» звуки. Маленькое кольцо над кружком — Feedback.</p>
<p><b>Matrix.</b> Первый пункт списка алгоритмов. Строка — модулятор, столбец — цель, OUT — уровень на выход. Кнопка
<b>Edit as Matrix</b> переносит текущий алгоритм в матрицу как стартовую точку. Диагональ (fb) — Feedback оператора, её можно
менять в любом режиме.</p>
<p>В режиме алгоритма громкость несущих автоматически выравнивается (чем больше несущих, тем тише каждая).</p>
<h3>32 алгоритма</h3>
<div class="algos">{algo_cards}</div>

<h2 class="pb">7. Огибающие</h2>
<p>У каждого оператора своя огибающая DAHDSR (вкладка <b>Envelope</b>, для выбранного оператора). Огибающая модулятора меняет
тембр во времени: быстрый спад модулятора даёт яркую атаку и мягкий хвост (колокол, электропиано), медленная атака — тембр
«раскрывается» (пэд).</p>{env_t}
<img class="shot" src="{img('ui_envelope.png')}" style="margin-top:2mm">

<h2 class="pb">8. Голос и унисон</h2>{voice_t}
<h2>9. Фильтр и Mod Env</h2>{filter_t}
<h2>10. LFO</h2>
<p>Два глобальных LFO. Формы: Sine, Triangle, Saw Up, Saw Down, Square, S&amp;H (ступеньки), Smooth Rnd (плавный случайный).
Rate — от 0.01 до 40 Гц. С включённым <b>Sync</b> скорость задаётся долями такта (от 8 тактов до 1/32, в том числе триоли),
а во время воспроизведения фаза привязана к позиции транспорта Ableton. Сами по себе LFO ничего не меняют —
назначьте их в Mod Matrix. Справа на панели LFO — индикатор текущего значения.</p>

<h2>11. Mod Matrix</h2>
<p>Восемь слотов: <b>источник → назначение × глубина</b> (±100%). Несколько слотов могут вести в одно назначение — значения складываются.</p>
{mod_t}{dst_t}

<h2 class="pb">12. Нижняя панель</h2>
<p><b>Drone.</b> Ноты «залипают»: отпущенная клавиша продолжает звучать. Можно добавлять ноты. Выключение Drone отпускает всё,
что не зажато физически. То же переключает плашка «Drone on» в визуализации.</p>
<p><b>Interpolation.</b> Как читаются семплы между отсчётами: <b>Hermite</b> — чище (по умолчанию), <b>Linear</b> — чуть грубее и
дешевле, иногда даёт приятную «зернистость».</p>
<p><b>Oversampling.</b> Синтез на повышенной частоте (1x / 2x / 4x). Сильная FM рождает обертоны выше слышимого диапазона,
которые «заворачиваются» в неприятные призвуки; оверсемплинг это убирает. Каждый шаг примерно удваивает нагрузку.
По умолчанию 2x.</p>
<p><b>Voices / CPU.</b> Сколько голосов звучит из 8 и какая доля времени аудиоблока уходит на Loom.</p>

<h2>13. Пресеты</h2>
<ul>
<li>Браузер — в шапке: ‹ › листают, клик по имени — список по папкам.</li>
<li>Сохранить: иконка настроек → <b>Save preset…</b>, указать имя и папку. Файлы <span class="mono">*.loom</span> лежат в
<span class="mono">~/Music/Loom/Presets</span>; семплы хранятся внутри файла, поэтому пресетом можно поделиться одним файлом.</li>
<li>Состояние со всеми семплами также сохраняется в проекте Ableton.</li>
</ul>
<h3>Заводские пресеты</h3>{factory}

<h2 class="pb">14. Рецепты</h2>
<h3>Тёмный дрон из любой записи</h3>
<ol><li>Алгоритм <b>24 Ring 6</b> или <b>26 Cross Pairs</b>. Drone — вкл.</li>
<li>На Op 1 — тональный семпл в Scrub, на Op 4 и Op 6 — шумовые текстуры в Sample PM, Keytrack выкл.</li>
<li>Ratio модуляторов 0.5–2, Depth 20–35%, Feedback на Op 6 ~40%.</li>
<li>Attack/Release всех операторов 5–10 с. Unison 6, Detune 20 ct.</li>
<li>Mod Matrix: LFO 1 (Smooth Rnd, 0.05–0.1 Гц) → FM Amount 30%; LFO 2 (Triangle, 0.03 Гц) → Op 1 Scan 50%.</li>
<li>Filter LP 24, Cutoff ~1.5 кГц. Сыграть одну-две низкие ноты и отпустить.</li></ol>
<h3>Стеклянный пэд</h3>
<ol><li>Алгоритм <b>6 Three Pairs</b>: три пары модулятор → несущая.</li>
<li>Несущие Ratio 1, 2, 4; модуляторы 3, 7, 1.5; Depth модуляторов 25–40%.</li>
<li>Attack 2 с, Release 4 с, Unison 4. LFO 1 0.1 Гц → FM Amount 25%.</li></ol>
<h3>Металл и колокола</h3>
<ol><li>Алгоритм <b>10 Five into One</b>.</li>
<li>Нецелые отношения: Fine 17–40%, один модулятор в Fixed freq 1–3 кГц.</li>
<li>Атака 0, Decay несущей 4–8 с, Sustain 0; модуляторы затухают быстрее несущей.</li></ol>
<h3>Гранулярное облако</h3>
<ol><li>Op 1 в Grain, длинный семпл. Size 150–400 мс, Density x5, Spray 40%.</li>
<li>Op 2 — синус Ratio 0.5, Depth 15% (медленный «изгиб» зёрен).</li>
<li>Mod Wheel → Op 1 Scan 80%: колесом путешествуете по семплу.</li></ol>

<h2>15. Управление мышью</h2>{gestures}
<h2>16. Если что-то не так</h2>{trouble}
<p class="dim" style="margin-top:6mm">Технические данные: 6 операторов, 32 алгоритма + матрица 6×6, полифония 8 голосов, унисон до 8,
подблоки модуляции 32 сэмпла, wavetable 2048 точек × 64 кадра с band-limited mip-уровнями, оверсемплинг на полуполосных IIR-фильтрах.</p>
</body></html>"""

(HERE / "Loom-Manual.html").write_text(HTML, encoding="utf-8")
print("ok", len(HTML))
