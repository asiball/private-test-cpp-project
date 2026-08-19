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
    spi-hal/src/ i2c-hal/src/ gpio/src/ libsensor/src/ libadxl345/src/ libmcp9808/src/ cli/src/ examples/ \
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
# テスト実行の失敗は非致命（デモ用途）。ビルド失敗は set -e で致命のまま。
./build/test-spihal/test_spi_driver \
    --gtest_output=xml:test-results/spihal-unit.xml \
    && echo "  spi-hal(SpiDriver) テスト: PASS" \
    || echo "  [警告] spi-hal(SpiDriver) テスト: 失敗あり"
./build/test-spihal/test_kernel_spi_driver \
    --gtest_output=xml:test-results/spihal-kernel-unit.xml \
    && echo "  spi-hal(KernelSpiDriver) テスト: PASS" \
    || echo "  [警告] spi-hal(KernelSpiDriver) テスト: 失敗あり"

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

# ── [4/6] 単体テスト: libsensor (MCP3008 + ADS1115) ─────────
echo "=== [4/6] 単体テスト (libsensor) ==="
cmake -S libsensor -B build/libsensor-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/libsensor-debug -j"$(nproc)"
cmake --install build/libsensor-debug --prefix /usr/local
ldconfig
# tests/unit/libsensor は test_ads1115 も含み、ads1115.hpp が ii2c_driver.hpp を
# 単純名 include するため i2c-hal/include が必須（CI の3系統と揃える。落とし穴 #2）。
cmake -S tests/unit/libsensor -B build/test-libsensor \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-I/workspace/libsensor/include -I/workspace/spi-hal/include -I/workspace/i2c-hal/include -I/workspace/tests/mocks"
cmake --build build/test-libsensor -j"$(nproc)"
# テスト実行の失敗はコンテナビルドを止めない（デモ用途のため非致命扱い。
# 一方ビルド/コンパイル失敗は set -e で致命のままにする）。
./build/test-libsensor/test_sensor \
    --gtest_output=xml:test-results/libsensor-unit.xml \
    && echo "  libsensor(MCP3008) テスト: PASS" \
    || echo "  [警告] libsensor(MCP3008) テスト: 失敗あり"
./build/test-libsensor/test_ads1115 \
    --gtest_output=xml:test-results/ads1115-unit.xml \
    && echo "  libsensor(ADS1115) テスト: PASS" \
    || echo "  [警告] libsensor(ADS1115) テスト: 失敗あり"

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
