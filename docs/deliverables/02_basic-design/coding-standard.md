# コーディング規約書 — embedded-device-suite

| 項目 | 内容 |
|---|---|
| ドキュメント番号 | STD-CODE-001 |
| バージョン | 1.0 |
| 対象 | 本リポジトリの C++17 コード（`kernel/` の C コードを除く）|
| 位置づけ | 納品文書（02_basic-design）。日常メモは [CLAUDE.md](../../../CLAUDE.md) の「コード規約」節を参照 |

> 本書は「規約は文書で配るだけでは守られない → フォーマッタ + 静的解析で機械的に強制する」という実務の定石を示す教材も兼ねる。

---

## 1. 言語標準・方針

- **言語標準は C++17 固定**。`std::span` / `std::expected` / `std::jthread` など C++20 以降の機能は使わない（`std::optional` は可）。
- 公開 API は基本 `noexcept`・`[[nodiscard]]`。失敗は例外でなく戻り値 / `std::optional` で表す。
- PIMPL + 依存注入(DI) を基本とし、公開ヘッダに実装詳細・OS 依存ヘッダを出さない（ABI 安定）。
- リソースは RAII（デストラクタで解放）。`goto err` 方式は使わない。
- 定数は `#define` でなく `enum class` / `constexpr`。実装内のマジックナンバーは無名 namespace の名前付き定数にする。

---

## 2. 命名規約

実コードは以下の規則に従う（新規コードもこれに合わせる）。`clang-tidy` の
`readability-identifier-naming` を `.clang-tidy` に設定すれば機械チェックできる
（現状は未設定。導入時はこの表を CheckOptions に落とし込む）。

| 対象 | 規則 | 例 |
|---|---|---|
| クラス / 構造体 | PascalCase（インターフェースは `I` 接頭辞） | `SpiDriver`, `Ads1115`, `ISpiDriver` |
| メソッド / 関数 | snake_case | `read_raw()`, `last_errno()`, `enable_tap_detection()` |
| メンバ変数 | snake_case + 末尾アンダースコア | `fd_`, `device_path_`, `last_errno_` |
| ローカル変数 / 引数 | snake_case | `speed_hz`, `channel` |
| 定数（`constexpr` / enum 値） | UPPER_SNAKE_CASE | `CHANNEL_COUNT`, `DEFAULT_ADDR`, `MCP3008_START_BIT` |
| enum class 型名 | PascalCase | `Gain`, `Edge` |
| 名前空間 | snake_case | `embedded`, `adxl345::reg` |
| ファイル名 | snake_case | `kernel_spi_driver.cpp` |
| マクロ | UPPER_SNAKE_CASE | `LOGI`, `LOG_OPEN` |

---

## 3. 書式（clang-format）

書式は [`.clang-format`](../../../.clang-format)（プロジェクトルート）に機械可読で定義する。要点:

- インデント 4 スペース（タブ不可）、行長 100、`NamespaceIndentation: None`
- 波括弧: **関数定義は Allman**（`{` を改行）、**クラス/構造体/制御文は K&R**（同行）
- ポインタ/参照は左寄せ（`uint8_t *p` ではなく既存コードに合わせ `PointerAlignment: Left`）
- `public:` / `private:` はクラスと同じ列（`AccessModifierOffset: -4`）
- 宣言・代入は縦揃え（`AlignConsecutiveDeclarations/Assignments`）
- `#include` は種別ごとにグループ化（自プロジェクト → システム）

> **強制状況（重要）**: 既存コードは手書きで一貫しているが、clang-format 18 の出力とは
> 細部（関数宣言の縦揃え等）で差が残るため、現状 `.clang-format` は **参考（advisory）**。
> CI で `clang-format --dry-run --Werror` をハードゲート化するには、一度
> `clang-format -i` で全ファイルを正規化する必要がある。これは `git blame` を汚し
> レビュー負荷も高いため、実施可否は別途判断する（issue #52）。それまでは
> **新規/変更箇所のみ** `.clang-format` に沿わせることを推奨する。

---

## 4. 静的解析

- **cppcheck**（`--enable=warning,performance,portability --std=c++17`）と **clang-tidy**
  （`.clang-tidy`：`bugprone-*` / 一部 `modernize-*` / `performance-*` / `readability-*`）を CI で実行。
  `bugprone-*` は warnings-as-errors。
- 対象ファイルは CI（`lint` ジョブ）と一致させる。新規コンポーネント追加時は両方に追加する
  （[CLAUDE.md](../../../CLAUDE.md) チェックリスト 4）。

---

## 5. 規格準拠方針（MISRA / CERT / AUTOSAR）

本プロジェクトは **MISRA C++ / AUTOSAR C++14 に準拠しない**。理由:

- **学習用途**であり、例外こそ使わないが動的確保（`std::make_unique` 等）・標準ライブラリ・
  RTTI を意図的に使用する（MISRA はこれらを制限する）。
- 代わりに **CERT C++ の安全方針の精神**（未定義動作の回避、RAII による資源管理、
  整数オーバーフロー・データレースの排除）を、cppcheck / clang-tidy / サニタイザー（ASAN/UBSAN/TSAN）
  で機械的に担保する。

実案件で MISRA/AUTOSAR 準拠が要求される場合は、本方針宣言を「準拠する」に切り替え、
準拠ツール（例: cppcheck の `--addon=misra`）と逸脱記録（deviation record）の運用を追加する。

---

## 6. バージョン履歴

| バージョン | 変更内容 |
|---|---|
| 1.0 | 初版。命名規約・書式・静的解析・MISRA/CERT 準拠方針を明文化（issue #52）|
