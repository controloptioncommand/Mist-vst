#!/usr/bin/env bash
# Пересобирает docs/Mist-manual.pdf: свежие скриншоты из рендер-теста → нарезка → печать HTML в PDF.
# Нужны: собранный MistRenderTest (см. CLAUDE.md), Python 3 с Pillow, Node.js с Playwright.
# На Linux рендер-тест запускается через xvfb-run.
set -euo pipefail
cd "$(dirname "$0")/../.."

TEST=./build/MistRenderTest_artefacts/Release/MistRenderTest
SHOTS=$(mktemp -d)
trap 'rm -rf "$SHOTS"' EXIT

cmake --build build --target MistRenderTest
RUN=()
if [[ "$(uname)" == "Linux" ]]; then RUN=(xvfb-run -a); fi
MIST_MANUAL_SHOTS=1 "${RUN[@]}" "$TEST" "$SHOTS" | tail -1

python3 docs/manual/crop.py "$SHOTS" docs/manual/img
NODE_PATH="${NODE_PATH:-$(npm root -g)}" node docs/manual/print.js docs/manual/Mist-manual.html docs/Mist-manual.pdf
echo "docs/Mist-manual.pdf: $(python3 -c 'import os; print(os.path.getsize("docs/Mist-manual.pdf") // 1024)') KB"
