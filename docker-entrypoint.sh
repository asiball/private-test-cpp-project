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
    driver/src/ lib/src/ cli/src/ \
    && echo "  cppcheck: 問題なし" \
    || echo "  [警告] cppcheck: 問題あり（続行）"

# ── [3/7] 単体テスト: driver ────────────────────────────────
echo "=== [3/7] 単体テスト (driver) ==="
cmake -S driver -B build/driver-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/driver-debug -j"$(nproc)"
cmake --install build/driver-debug --prefix /usr/local
cmake -S tests/unit/driver -B build/test-driver \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-I/workspace/driver/include"
cmake --build build/test-driver -j"$(nproc)"
mkdir -p test-results
./build/test-driver/test_spi_driver \
    --gtest_output=xml:test-results/driver-unit.xml \
    && echo "  driver テスト: PASS" \
    || echo "  [警告] driver テスト: 失敗あり"

# ── [4/7] 単体テスト: lib ───────────────────────────────────
echo "=== [4/7] 単体テスト (lib) ==="
cmake -S lib -B build/lib-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/lib-debug -j"$(nproc)"
cmake --install build/lib-debug --prefix /usr/local
ldconfig
cmake -S tests/unit/lib -B build/test-lib \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-I/workspace/lib/include -I/workspace/tests/mocks"
cmake --build build/test-lib -j"$(nproc)"
./build/test-lib/test_device \
    --gtest_output=xml:test-results/lib-unit.xml \
    && echo "  lib テスト: PASS" \
    || echo "  [警告] lib テスト: 失敗あり"

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
echo "  バイナリ         : build/cli/device-ctl, build/lib/libdevice.so"
echo "  テスト結果       : test-results/"
echo "  APIドキュメント  : docs/doxygen/html/index.html"
echo "  PDF              : output/pdf/"
echo "  Excelドキュメント: docs/"
