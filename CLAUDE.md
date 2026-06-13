# CLAUDE.md

このファイルは、本リポジトリで作業する際に**先に知っておくと事故を防げること**だけをまとめたものです。
全体構成・各コンポーネントの役割は [README.md](README.md) と [docs/](docs/) を参照してください（ここでは重複させません）。

---

## このプロジェクトの前提

- **言語標準は C++17 固定**。`std::span` / `std::expected` / `std::jthread` など C++20 以降の機能は使わない（`std::optional` は可）。
- 読者・貢献者には**組み込みで C 中心の開発者**を想定。新規の C++ 機能は「導入・ステップアップ」の位置づけで、不必要に重くしない。
- 各コンポーネントは**独立**。一部を削除してもプロジェクトはビルド・動作する設計（→ [docs/adr/0001-optional-independent-components.md](docs/adr/0001-optional-independent-components.md)）。

---

## ⚠️ 最初に踏みやすい落とし穴

> これらの改善案は GitHub Issues（[#20 テストの CMake 統合](https://github.com/asiball/private-test-cpp-project/issues/20) / [#21 CI 一元化](https://github.com/asiball/private-test-cpp-project/issues/21) ほか）に今後の課題として整理してある。

### 1. テストは「スタンドアロン configure + install 済みライブラリの名前解決」方式
`tests/unit/*` と `tests/integration` は**トップレベル CMake からビルドされない**。
各ライブラリを先に `--install` してから、テストディレクトリを個別に configure し、
ライブラリは**インストール済みのものを名前でリンク**する。

そのため、テストの **include パスは `target_include_directories` ではなく CI 側の
`-DCMAKE_CXX_FLAGS="-I.../include"` 経由で効く**（CMakeLists 内の `${CMAKE_SOURCE_DIR}/...` は
スタンドアロンビルドでは別パスに解決され、実質効かない）。

→ **テストが新しいヘッダを include するなら、CI の該当ステップに `-I` を足す必要がある。**

> **別経路（推奨・issue #20 / #21）**: トップレベルから `-DBUILD_TESTING=ON`（または
> `CMakePresets.json` の `debug` / `coverage` / `asan` / `tsan` プリセット）で configure すると、
> 全コンポーネント + テストを**同一ツリーで直接ビルド**し `ctest` で実行できる。この経路では
> include は in-tree target から伝播するため**上記の `-I` 手渡しは不要**（落とし穴 #1/#2 を回避）。
> CI の `integrated-tests` ジョブがこの経路を matrix で回す。レガシーな per-component +
> install + standalone 方式（下記の各ジョブ）も当面併存させている。

### 2. テスト用 include は CI の 3 系統すべてに反映する（レガシー standalone 経路）
`.github/workflows/ci.yml` には同じテストを違うフラグでビルドする系統が複数ある：

| ジョブ / ステップ | フラグ |
|---|---|
| `build-and-test` | 通常 Debug |
| `coverage` | `--coverage` |
| `sanitizer` | ASAN+UBSAN / TSAN |

**1 系統だけ直すと他で落ちる。** 例: `tests/unit/libsensor` は `test_ads1115`（→ `i2c-hal/include` 依存）を
含むため、`build-and-test` だけでなく `coverage` と `sanitizer` の libsensor テストにも
`-I i2c-hal/include` が必要（過去にこの漏れで CI が赤になった実績あり）。

### 3. GTest はソースからビルドして install する前提
CI は `/usr/src/googletest` を `cmake` でビルドして `/usr/local` に install してからテストする。
ローカル再現時も同様（`apt` の `libgtest-dev` はヘッダのみのことがある）。

### 4. SBOM は手動メンテ
新コンポーネントを足したら `tools/sbom-metadata.json` に **packages と relationships を追記** し、
`python3 tools/generate-sbom.py` で `sbom.spdx` / `sbom.cdx.json` を再生成する。
CI の `Verify SBOM consistency` は `--verify` で**メタデータと生成物の整合**をチェックする
（ソースツリーとの網羅性は見ない）。スキーマキーは `spdx_id` / `bom_ref` / `cdx_type`。

---

## コード規約

- **PIMPL + 依存注入(DI)** を基本に。公開ヘッダに実装詳細・OS 依存ヘッダを出さない（ABI 安定のため）。
- ハードウェア/OS 境界は**純粋仮想インターフェース**（`ISpiDriver` / `II2cDriver` 等）で抽象化し、テストはモックを注入する。
- リソースは **RAII**（デストラクタで解放）。`goto err` 方式は使わない。
- 公開 API は基本 `noexcept`、返り値は `[[nodiscard]]`。失敗は例外でなく戻り値 / `std::optional` で表す。
- 定数は `#define` でなく **`enum class` / `constexpr`**。実装内のマジックナンバーは
  **無名 namespace の名前付き定数**にする（例: `MCP3008_START_BIT`）。
- ログは `common/include/logger.hpp` の `LOGI/LOGW/LOGE/LOGD` を使う。
- **公開 API の `noexcept` は読み出し系まで一貫させる**（例外を投げず `std::optional` / 戻り値で失敗を表すため）。
  例外: 内部で `std::thread` を生成するなど送出しうるものは非 `noexcept`（例: `Sensor::read_raw_async`）。
- **別コンポーネントのヘッダ include**：`sensor.hpp` / `adxl345.hpp` が
  `#include "../../spi-hal/include/ispi_driver.hpp"` のように相対パスで指すのは
  **意図的**（スタンドアロンテストが `-I` 無しで解決できるようにするため。落とし穴 #1）。
  整理目的で単純名 include に変えると、テストの `-I` を 3 系統すべてに足す必要が出るので注意。

---

## ドキュメントの図（mermaid）

図は **mermaid** で書く。GitHub はクライアント側描画なので**構文エラーが push/PR 時に検知されず**、
閲覧して初めて壊れに気づく（過去に `[/dev/spidev0.0]` がパーサに平行四辺形構文と誤認され崩れた実績）。

そのため記法ルールを暗記するのではなく、**機械で弾く**：

- 検証本体は `tools/check-mermaid.sh`（Docker の `minlag/mermaid-cli` で全 `*.md` の mermaid を構文チェック）。
  **CI（`docs` ジョブ）で常に実行**され、失敗で red。
- ローカルは **pre-push フック**で push 差分の `.md` だけを検証する。各自一度だけ有効化:
  ```sh
  git config core.hooksPath .githooks
  ```
  Docker が無ければ警告してスキップ（強制は CI 側）。手動実行は `bash tools/check-mermaid.sh`。

---

## 新しいコンポーネントを追加するときのチェックリスト

1. ディレクトリを作り `CMakeLists.txt` を置く（独立してビルド/インストールできる単位にする）。
2. トップ `CMakeLists.txt` の `foreach(_component ...)` リストに**依存順で**追加（EXISTS ガードで「あるものだけ」ビルド）。
3. `tests/unit/<name>/` にテスト + `CMakeLists.txt` + `test-cases.md`（既存の standalone 方式を踏襲）。実機が要るテストは `GTEST_SKIP()`。`test-cases.md` の各 ID は `TEST(Suite, Name)` と 1:1 対応させ、`docs/deliverables/06_test/test-plan.md` §4 のスイート一覧にも 1 行追加する。
   - **target 名は全テストディレクトリで一意にする**（統合経路では同一ツリーで全テストを add するため衝突する。例: 結合テストは `integration_` 前置）。
   - トップ `CMakeLists.txt` の `BUILD_TESTING` ブロックに `add_subdirectory(tests/unit/<name>)` を**対応 target の存在ガード付きで**追加する（統合経路 / issue #20）。
4. `.github/workflows/ci.yml`：ビルド/テストステップ追加、**cppcheck と clang-tidy の対象ファイルに追加**。
   - 統合経路（`integrated-tests` ジョブ）は preset で全テストを回すので個別の追記は不要。
   - レガシー standalone 経路を使う場合のみ、テスト include を **3 系統すべて**に反映（落とし穴 #2）。
5. `Doxyfile` の `INPUT` にヘッダディレクトリを追加。
6. `tools/sbom-metadata.json` に packages + relationships を追記し SBOM 再生成（落とし穴 #4）。
7. リリース対象なら `.github/workflows/release.yml` / `sbom.yml` のタグトリガに `<name>/v*` を追加。
8. README / `docs/guides/learning-guide.md` に導線を追加。

---

## コミット / PR

- **Conventional Commits 必須**。`feat|fix|docs|refactor|test|ci|chore|build`（`(scope)` 可）。
  CI の `commit-lint` が **PR タイトルと全コミットメッセージ**を検証する。
- コンポーネントごとに独立したタグ（`spi-hal/v*` / `libsensor/v*` / `cli/v*` …）でリリースする。

---

## ローカル検証の最短コマンド

```sh
# フルビルド（全コンポーネント、トップレベル）
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)

# テスト（推奨・統合経路）: 全コンポーネント + テストを同一ツリーでビルドし ctest 実行。
# -I 手渡し不要（落とし穴 #1/#2 を回避）。preset は debug | coverage | asan | tsan。
cmake --preset debug && cmake --build --preset debug -j$(nproc) && ctest --preset debug
# preset を使わない場合: cmake -S . -B build -DBUILD_TESTING=ON && cmake --build build && (cd build && ctest)

# 静的解析（CI lint 相当）
cppcheck --enable=warning,performance,portability --std=c++17 \
  --suppress=missingIncludeSystem --error-exitcode=1 \
  spi-hal/src/ i2c-hal/src/ gpio/src/ libsensor/src/ libadxl345/src/ cli/src/ examples/

# SBOM 整合チェック
python3 tools/generate-sbom.py --verify

# ドキュメントの mermaid 図を検証（Docker 必須。無ければ警告スキップ）
bash tools/check-mermaid.sh
```

レガシーな standalone 経路（「ライブラリを install → テストを standalone configure（`-I` 付き）」）も
併存しており、`.github/workflows/ci.yml` の各ステップが最も正確なリファレンス（落とし穴 #1/#2）。
