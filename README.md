# embedded-device-suite

Linux組み込みデバイス向けモノレポ。ドライバ・共有ライブラリ・CLIツールを一元管理する。

> **このプロジェクトの目的**
>
> 実案件レベルの組み込みSW開発**運用フロー**をまるごと体験・学習するための例として構築されています。
> C++の設計パターンだけでなく、仕様策定→設計→実装→テスト→CI/CD→納品 という
> **開発ライフサイクル全体**が一つのリポジトリで追えます。
> まずは [学習ガイド](docs/guides/learning-guide.md) をお読みください。

---

## 開発ライフサイクル

本プロジェクトの全フェーズが `docs/` に揃っています。以下の流れで一貫して追うことができます。

```
┌─────────────────────────────────────────────────────────────────┐
│  フェーズ          ドキュメント                  成果物          │
├─────────────────────────────────────────────────────────────────┤
│  01 要件定義  →  requirements-spec.md        機能要件・制約     │
│  02 基本設計  →  system-architecture.md     システム構成図      │
│  03 詳細設計  →  spihal-design.md / libsensor-design.md  クラス設計 │
│  04 API仕様   →  spi-driver-api.md / libsensor-api.md  公開API   │
│  05 IF仕様    →  spi-hardware-if.md          HW接続仕様         │
│  06 テスト    →  test-plan.md + tests/*/test-cases.md           │
│  07 納品      →  release-notes/                                 │
└─────────────────────────────────────────────────────────────────┘
```

各フェーズのドキュメントとコードが紐付いており、「なぜこの設計にしたか」を仕様書まで遡って確認できます。

---

## モノレポ構成

```
.
├── spi-hal/         # SPI HAL（静的ライブラリ: libspihal.a）
├── libsensor/       # libsensor（動的共有ライブラリ: libsensor.so）
├── cli/             # device-ctl（CLIツール）
├── kernel/          # Linux カーネルドライバ（my_spi_driver.ko）
├── tests/
│   ├── mocks/       #   MockSpiDriver（テスト用）
│   ├── unit/        #   単体テスト（spi-hal / libsensor）
│   └── integration/ #   結合テスト（実機必須）
├── docs/            # プロジェクトドキュメント一式（01〜07フェーズ）
├── tools/           # SBOM 生成スクリプト（generate-sbom.py 等）
├── .github/         # GitHub Actions CI
├── Doxyfile         # Doxygen 設定
├── Dockerfile.build # Docker ビルド環境
└── CMakeLists.txt   # トップレベル CMake
```

### なぜ3コンポーネントに分割するか

| コンポーネント | 種別 | 理由 |
|---|---|---|
| `spi-hal/` (libspihal) | 静的ライブラリ | Linux SPI 依存コードを隔離。HWが変わっても `libsensor/` 以上への影響を最小化 |
| `libsensor/` (libsensor) | 共有ライブラリ | PIMPLでABIを安定化。ライブラリバージョンを独立して管理・リリース可能にする |
| `cli/` (device-ctl) | 実行バイナリ | 対話型CLIツール。起動後メニューからMCP3008の各チャネル読み出しを繰り返し実行できる。 |

コンポーネントごとに独立した git タグを持ち（`spi-hal/v1.1.0`、`libsensor/v1.1.0` 等）、差分ビルドで影響範囲を最小化します。

---

## CI/CDパイプライン

```
コード変更
    │
    ├─ push / PR作成
    │       │
    │       ▼
    │   lint（cppcheck / clang-tidy）
    │       │
    │       ▼
    │   build（変更コンポーネントのみ差分ビルド）
    │       │
    │       ▼
    │   test:unit（GMock・実機不要）→ カバレッジレポート
    │       │
    │       ▼
    │   docs（Doxygen HTML / PDF）
    │
    └─ タグ push（spi-hal/vX.Y.Z 等）
            │
            ▼
        package:release（.tar.gz アーカイブ生成）
```

| プラットフォーム | 設定ファイル | 特徴 |
|---|---|---|
| GitHub Actions | `.github/workflows/ci.yml` | push/PRで自動実行（ビルド・テスト・カバレッジ・Doxygen） |

---

## テスト戦略

| レベル | 対象 | ツール | 自動化 | 実行環境 |
|---|---|---|---|---|
| 単体テスト (UT) | SpiDriver / Sensor クラス | Google Test + GMock | ✓ CI | Docker（実機不要） |
| 結合テスト (IT) | MCP3008 実機読み出し | Google Test | 一部 | Raspberry Pi 3B+ + MCP3008 |
| 受入テスト (AT) | システム全体 | 手動 + チェックリスト（Markdown） | - | 実機 + 顧客確認 |

カバレッジ目標: 主要モジュール **80%以上**（gcovr/lcovで計測）

---

## ドキュメント読み進めガイド

ドキュメントは「学習者向けガイド」と「プロジェクト成果物（納品物サンプル）」に分けています。

```
docs/
├── guides/             ← 学習者向けガイド（このリポジトリの読み方など）
│   ├── learning-guide.md
│   └── sbom-guide.md
├── deliverables/       ← プロジェクト成果物（要件→設計→テスト→納品の順）
│   ├── 01_requirements/   何を作るかを決める（要件定義書）
│   ├── 02_basic-design/   どう作るかの全体像（システム構成・アーキテクチャ）
│   ├── 03_detailed-design/クラス設計の詳細（SpiDriver・Sensor）
│   ├── 04_api-spec/       公開 API の仕様書（使う側の視点）
│   ├── 05_interface-spec/ ハードウェアとのインターフェース仕様
│   ├── 06_test/           テスト計画・仕様書
│   └── 07_delivery/       リリースノート・納品物チェックリスト
├── wiki/               ← 運用情報（リリースマトリックス等）
└── assets/             ← Doxygen 用 CSS 等
```

コードを読む場合のお勧め順序:
1. `spi-hal/include/ispi_driver.hpp` — インターフェース設計
2. `spi-hal/include/spi_driver.hpp` / `spi-hal/src/spi_driver.cpp` — 実機ドライバ実装
3. `libsensor/include/sensor.hpp` / `libsensor/src/sensor.cpp` — PIMPL + 非同期API
4. `tests/mocks/mock_spi_driver.hpp` — モック実装
5. `tests/unit/` — ユニットテスト

---

## クイックスタート

### Docker を使う（推奨）

```bash
# ビルド・テスト・静的解析・Doxygenを一括実行
./docker-build.sh
```

### ローカルビルド

```bash
# 全コンポーネントビルド（Release）
cmake -B build -DCMAKE_BUILD_TYPE=Release .
cmake --build build

# Debug ビルド（ログが stderr にも出力される）
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DDEBUG=1 .
cmake --build build
```

### 単体テストの実行

```bash
# spi-hal 単体テスト
cmake -S spi-hal -B build/spihal -DCMAKE_BUILD_TYPE=Debug
cmake --build build/spihal
cmake -S tests/unit/spi-hal -B build/test-spihal
cmake --build build/test-spihal
./build/test-spihal/test_spi_driver

# libsensor 単体テスト
cmake -S libsensor -B build/libsensor -DCMAKE_BUILD_TYPE=Debug
cmake --build build/libsensor
cmake -S tests/unit/libsensor -B build/test-libsensor
cmake --build build/test-libsensor
./build/test-libsensor/test_sensor
```

### Doxygen ドキュメントの生成

```bash
doxygen Doxyfile
# 出力先: docs/doxygen/html/index.html
```

### cppcheck（静的解析）

```bash
cppcheck --enable=warning,performance,portability --std=c++17 \
         --suppress=missingIncludeSystem --error-exitcode=1 \
         spi-hal/src/ libsensor/src/ cli/src/
```

### device-ctl の使い方

```bash
# デフォルトデバイス (/dev/spidev0.0) / Vref 3.3V で起動
./build/device-ctl

# デバイスパスを指定して起動
./build/device-ctl -d /dev/spidev0.1

# Vref を指定して起動（例: 5.0V）
./build/device-ctl --vref 5.0

# 非同期readモードで起動
./build/device-ctl --async

# バージョン確認
./build/device-ctl --version
```

起動後は対話メニューが表示され、MCP3008 の各チャネル（0〜7）の読み出しを繰り返し実行できます。
読み出し値は 10bit raw（0〜1023）と Vref から換算した電圧の両方を表示します。
バックグラウンドで1分ごとに CH0 を自動読み出しし、次のメニュー表示時に
`[MONITOR]` プレフィックス付きで結果を表示します。

```
device-ctl 対話モード (デバイス: /dev/spidev0.0, Vref=3.30 V)

[1] チャネル指定読み出し
[2] 全チャネルスキャン
[3] 終了
選択: 1
  チャネル (0-7): 0
CH0  raw=512  voltage=1.650 V

[MONITOR] CH0 raw=512 voltage=1.650 V

[1] チャネル指定読み出し
[2] 全チャネルスキャン
[3] 終了
選択: 3
終了します。
```

---

## ビルド要件

| ツール | バージョン |
|---|---|
| GCC | 7.5 以上（C++17対応） |
| CMake | 3.10 以上 |
| Google Test | 1.14（単体テスト用） |
| Doxygen | 1.9 以上（ドキュメント生成用） |
| Python 3 | （generate-sbom.py 等のスクリプト使用時のみ） |

---

## タグ命名規則

```
<component>/v<MAJOR>.<MINOR>.<PATCH>

例:
  spi-hal/v1.0.0
  libsensor/v1.0.0
  cli/v1.0.0
```

タグがないと `--version` が `v0.0.0-unknown` を表示するため、
リリース時は必ずタグを作成すること（詳細は `CONTRIBUTING.md` 参照）。

---

## C++設計パターン

実装で使用している主なC++パターンの一覧。詳細は [学習ガイド](docs/guides/learning-guide.md) を参照。

| パターン | 場所 | 概要 |
|---|---|---|
| **RAII** | `spi-hal/src/spi_driver.cpp` | デストラクタで fd を自動 close |
| **PIMPL イディオム** | `libsensor/src/sensor.cpp` | `Sensor::Impl` で実装を隠蔽し ABI を安定化 |
| **インターフェース分離** | `spi-hal/include/ispi_driver.hpp` | 純粋仮想クラス `ISpiDriver` で実機とモックを交換可能に |
| **依存注入 (DI)** | `libsensor/include/sensor.hpp` | テスト用コンストラクタで `ISpiDriver*` を外部注入 |
| **`[[nodiscard]]` / `noexcept`** | `spi-hal/include/ispi_driver.hpp` | 戻り値無視の防止と例外を使わないエラー設計 |
| **ロガーマクロ** | `spi-hal/include/logger.hpp` | DEBUGビルドは stderr+syslog、RELEASEは syslog のみ |
| **バージョン自動生成** | `spi-hal/include/version.hpp.in` | CMake `configure_file` + `git describe` でビルド情報を埋め込み |
| **Doxygen スニペット** | `spi-hal/include/ispi_driver.hpp` | `@snippet` でテストコードをAPIドキュメントに引用 |
| **GMock** | `tests/mocks/mock_spi_driver.hpp` | `MOCK_METHOD` で実機不要のユニットテスト |

---

## ドキュメント

### 学習ガイド（`docs/guides/`）

- [学習ガイド（C++ パターン総合）](docs/guides/learning-guide.md)
- ツール別ガイド（`docs/guides/tooling/`）:
  [CMake](docs/guides/tooling/cmake-guide.md) ・
  [Google Test/GMock](docs/guides/tooling/gtest-guide.md) ・
  [Doxygen](docs/guides/tooling/doxygen-guide.md) ・
  [静的解析](docs/guides/tooling/static-analysis-guide.md) ・
  [カバレッジ](docs/guides/tooling/coverage-guide.md) ・
  [サニタイザー](docs/guides/tooling/sanitizers-guide.md) ・
  [Linux カーネルモジュール](docs/guides/tooling/kernel-module-guide.md)
- [SBOM ガイド](docs/guides/sbom-guide.md)

### 案件成果物（`docs/deliverables/`）

- [API仕様書 — libsensor](docs/deliverables/04_api-spec/libsensor-api.md)
- [API仕様書 — SpiDriver](docs/deliverables/04_api-spec/spi-driver-api.md)

### 運用情報（`docs/wiki/`）

- [リリースマトリックス](docs/wiki/release-matrix.md)
