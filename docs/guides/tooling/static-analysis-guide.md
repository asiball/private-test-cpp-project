# 静的解析ガイド（cppcheck / clang-tidy / clang-format）

| 項目 | 内容 |
|---|---|
| 対象読者 | C++ の静的解析ツールを CI に組み込みたい方 |
| 取り上げるツール | cppcheck、clang-tidy、clang-format |
| 本プロジェクトでの使用箇所 | `.clang-format`、`.clang-tidy`、`.github/workflows/ci.yml` の lint ジョブ |

---

## 各ツールの役割

| ツール | 目的 | 検出例 |
|---|---|---|
| **cppcheck** | 軽量な静的バグ検出 | 未初期化変数、解放後参照、無限ループ |
| **clang-tidy** | Clang ベースの高度な解析 + 自動修正 | 現代 C++ ベストプラクティス違反、性能低下パターン |
| **clang-format** | コード整形 | インデント、ブレース位置、空白の統一 |

3 つは「検出」「修正提案」「整形」と役割が分かれているため、組み合わせて使うのが定石。

---

## 構文の基本

### cppcheck

```bash
cppcheck \
    --enable=warning,performance,portability \
    --std=c++17 \
    --suppress=missingIncludeSystem \
    --error-exitcode=1 \
    spi-hal/src/ libsensor/src/ cli/src/
```

| オプション | 内容 |
|---|---|
| `--enable=warning,performance,portability` | 検査対象カテゴリ |
| `--std=c++17` | 言語規格 |
| `--suppress=missingIncludeSystem` | 標準ヘッダの不在警告を抑制 |
| `--error-exitcode=1` | エラー検出時に終了コード 1（CI でフェイル） |

### clang-tidy

```bash
# compile_commands.json が必要（CMake で生成）
cmake -B build -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

clang-tidy \
    -p build/ \
    --checks='-*,bugprone-*,modernize-use-nullptr,modernize-use-override,performance-*' \
    --warnings-as-errors='bugprone-*' \
    spi-hal/src/spi_driver.cpp libsensor/src/sensor.cpp
```

| オプション | 内容 |
|---|---|
| `-p` | `compile_commands.json` のあるディレクトリ |
| `--checks` | 有効化するチェック（`-*` で全部 OFF にしてから列挙）|
| `--warnings-as-errors` | 指定カテゴリは error 扱い |

設定は `.clang-tidy` ファイルにも記述可能（コマンドラインより優先度低）。

### clang-format

```bash
# 個別ファイル整形（-i は in-place）
clang-format -i spi-hal/src/spi_driver.cpp

# プロジェクト全体に適用
find spi-hal libsensor cli -name '*.cpp' -o -name '*.hpp' \
    | xargs clang-format -i
```

スタイルは `.clang-format` で定義（YAML 形式、`BasedOnStyle: Google` などから派生）。

---

## 本プロジェクトでの使われ方

### CI ジョブ

`.github/workflows/ci.yml` の `lint` ジョブで cppcheck + clang-tidy を実行。
両方とも違反検出で CI 失敗する設定:

```yaml
- name: Run cppcheck
  run: |
    cppcheck \
      --enable=warning,performance,portability \
      --std=c++17 \
      --suppress=missingIncludeSystem \
      --error-exitcode=1 \
      spi-hal/src/ i2c-hal/src/ gpio/src/ libsensor/src/ libadxl345/src/ libmcp9808/src/ cli/src/ examples/

- name: Run clang-tidy
  run: |
    cmake -B build-tidy -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug
    clang-tidy -p build-tidy/ \
      spi-hal/src/spi_driver.cpp spi-hal/src/kernel_spi_driver.cpp \
      i2c-hal/src/i2c_driver.cpp gpio/src/gpio_line.cpp \
      libsensor/src/sensor.cpp libsensor/src/ads1115.cpp \
      libadxl345/src/adxl345.cpp libmcp9808/src/mcp9808.cpp \
      cli/src/main.cpp \
      examples/ads1115_alert_demo.cpp examples/adxl345_basic.cpp \
      examples/gpio_edge_demo.cpp examples/i2c_raw_demo.cpp examples/kernel_spi_demo.cpp
```

cppcheck は `src/` を持つ**全コンポーネント + examples**（8 ディレクトリ）を対象にする。
clang-tidy は `--checks` を CLI で指定せず、**`.clang-tidy` を有効チェックの単一の情報源**とする
方針（コマンドラインで `--checks` を指定すると `.clang-tidy` の設定が上書きされ、ローカル実行と
CI で適用チェックが食い違うため）。対象ファイルは cppcheck の対象ディレクトリと揃え、
`kernel_spi_driver.cpp` と `examples/` の各デモも個別に列挙している（現在 14 ファイル）。

### 設定ファイル

| ファイル | 内容 |
|---|---|
| `.clang-format` | インデント・ブレース等の整形ルール |
| `.clang-tidy`   | 有効化するチェックとオプション |

### ローカルでの素早い確認

```bash
# pre-commit 相当
clang-format --dry-run --Werror --style=file spi-hal/src/*.cpp \
  && cppcheck --enable=warning,performance --std=c++17 spi-hal/src/ \
  && echo "OK"
```

---

## 運用 Tips

- **checks の絞り込みは重要**: clang-tidy 全 ON にすると数千行の警告が出る。
  `bugprone-*` / `modernize-*` / `performance-*` あたりから段階的に有効化
- **NOLINT で局所抑制**:

  ```cpp
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  int set_color(uint8_t r, uint8_t g, uint8_t b);
  ```

- **clang-format の差分プレビュー**: `clang-format --dry-run --Werror` を CI で実行すると、
  整形漏れを fail にできる（自動 fix はしない）
- **cppcheck の偽陽性**: テンプレート絡みで誤検知することがあるため、
  `--suppress=...` で抑制 / `// cppcheck-suppress xxx` で局所抑制
