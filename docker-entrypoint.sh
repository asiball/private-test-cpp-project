#!/usr/bin/env bash
set -euo pipefail
cd /workspace

# ── [1/7] CMakeビルド (Release) ─────────────────────────────
echo "=== [1/7] CMakeビルド (Release) ==="
rm -f build/CMakeCache.txt
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"

# ── [2/7] cppcheck 静的解析 ─────────────────────────────────
echo "=== [2/7] cppcheck 静的解析 ==="
cppcheck \
    --enable=warning,performance,portability \
    --std=c++17 \
    --suppress=missingIncludeSystem \
    --error-exitcode=1 \
    spi-hal/src/ libsensor/src/ cli/src/ \
    && echo "  cppcheck: 問題なし" \
    || echo "  [警告] cppcheck: 問題あり（続行）"

# ── [3/7] 単体テスト: spi-hal ───────────────────────────────
echo "=== [3/7] 単体テスト (spi-hal) ==="
cmake -S spi-hal -B build/spihal-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/spihal-debug -j"$(nproc)"
cmake --install build/spihal-debug --prefix /usr/local
cmake -S tests/unit/spi-hal -B build/test-spihal \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-I/workspace/spi-hal/include"
cmake --build build/test-spihal -j"$(nproc)"
mkdir -p test-results
./build/test-spihal/test_spi_driver \
    --gtest_output=xml:test-results/spihal-unit.xml \
    && echo "  spi-hal テスト: PASS" \
    || echo "  [警告] spi-hal テスト: 失敗あり"

# ── [3b/7] サニタイザービルド (ASAN + UBSAN) ────────────────
echo "=== [3b/7] サニタイザービルド (ASAN + UBSAN) ==="
cmake -S . -B build/sanitized \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build/sanitized -j"$(nproc)"
./build/sanitized/cli/device-ctl --version \
    && echo "  サニタイザービルド: OK" \
    || echo "  [警告] サニタイザービルド: 起動失敗（続行）"

# ── [4/7] 単体テスト: libsensor ─────────────────────────────
echo "=== [4/7] 単体テスト (libsensor) ==="
cmake -S libsensor -B build/libsensor-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/libsensor-debug -j"$(nproc)"
cmake --install build/libsensor-debug --prefix /usr/local
ldconfig
cmake -S tests/unit/libsensor -B build/test-libsensor \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-I/workspace/libsensor/include -I/workspace/tests/mocks"
cmake --build build/test-libsensor -j"$(nproc)"
./build/test-libsensor/test_sensor \
    --gtest_output=xml:test-results/libsensor-unit.xml \
    && echo "  libsensor テスト: PASS" \
    || echo "  [警告] libsensor テスト: 失敗あり"

# ── [5/7] Doxygen ───────────────────────────────────────────
echo "=== [5/7] Doxygen ==="
mkdir -p docs/doxygen
doxygen Doxyfile

# ── [6/7] pandoc Markdown → PDF ─────────────────────────────
echo "=== [6/7] pandoc Markdown → PDF ==="
mkdir -p output/pdf
for f in \
    docs/01_requirements/requirements-spec.md \
    docs/02_basic-design/system-architecture.md \
    docs/03_detailed-design/driver-design.md \
    docs/04_api-spec/libdevice-api.md \
    docs/05_interface-spec/spi-hardware-if.md \
    docs/06_test/test-plan.md \
    docs/07_delivery/release-notes/v1.1.0.md; do
    name=$(basename "$f" .md)
    if [ -f "$f" ]; then
        pandoc "$f" \
            --pdf-engine=xelatex \
            -V mainfont="Noto Serif CJK JP" \
            -V geometry:margin=25mm \
            -o "output/pdf/${name}.pdf" \
            && echo "  converted: ${name}.pdf" \
            || echo "  [警告] ${name}.pdf 変換失敗（スキップ）"
    else
        echo "  [スキップ] ${f} が見つかりません"
    fi
done

# ── [7/7] Excel / CSV ドキュメント生成 ──────────────────────
echo "=== [7/7] Excelドキュメント生成 (tools/gen_docs.py) ==="
python3 tools/gen_docs.py

echo ""
echo "=== 完了 ==="
echo "  バイナリ         : build/cli/device-ctl, build/libsensor/libsensor.so"
echo "  テスト結果       : test-results/"
echo "  APIドキュメント  : docs/doxygen/html/index.html"
echo "  PDF              : output/pdf/"
echo "  Excelドキュメント: docs/"
