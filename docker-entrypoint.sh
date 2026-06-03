#!/usr/bin/env bash
set -euo pipefail
cd /workspace

# ── [1/6] CMakeビルド (Release) ─────────────────────────────
echo "=== [1/6] CMakeビルド (Release) ==="
rm -f build/CMakeCache.txt
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"

# ── [2/6] cppcheck 静的解析 ─────────────────────────────────
echo "=== [2/6] cppcheck 静的解析 ==="
cppcheck \
    --enable=warning,performance,portability \
    --std=c++17 \
    --suppress=missingIncludeSystem \
    --error-exitcode=1 \
    spi-hal/src/ i2c-hal/src/ gpio/src/ libsensor/src/ libadxl345/src/ cli/src/ examples/ \
    && echo "  cppcheck: 問題なし" \
    || echo "  [警告] cppcheck: 問題あり（続行）"

# ── [3/6] 単体テスト: spi-hal ───────────────────────────────
echo "=== [3/6] 単体テスト (spi-hal) ==="
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

# ── [3b/6] サニタイザービルド (ASAN + UBSAN) ────────────────
echo "=== [3b/6] サニタイザービルド (ASAN + UBSAN) ==="
cmake -S . -B build/sanitized \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build/sanitized -j"$(nproc)"
./build/sanitized/cli/device-ctl --version \
    && echo "  サニタイザービルド: OK" \
    || echo "  [警告] サニタイザービルド: 起動失敗（続行）"

# ── [4/6] 単体テスト: libsensor ─────────────────────────────
echo "=== [4/6] 単体テスト (libsensor) ==="
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

# ── [5/6] Doxygen ───────────────────────────────────────────
echo "=== [5/6] Doxygen ==="
mkdir -p docs/doxygen
doxygen Doxyfile

# ── [6/6] ドキュメント生成（pandoc → PDF + Word）────────────
echo "=== [6/6] ドキュメント生成 (pandoc → PDF + Word) ==="
# docs/deliverables 配下を走査して PDF と Word(.docx) を生成する（索引・テンプレは自動除外）。
# Doxygen HTML は [5/6] で生成済みのため、ここでは pdf docx のみ指定する。
bash tools/build-docs.sh pdf docx

echo ""
echo "=== 完了 ==="
echo "  バイナリ         : build/cli/device-ctl, build/libsensor/libsensor.so"
echo "  テスト結果       : test-results/"
echo "  APIドキュメント  : docs/doxygen/html/index.html"
echo "  PDF / Word       : output/pdf/, output/docx/"
