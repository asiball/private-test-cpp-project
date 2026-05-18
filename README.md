# embedded-device-suite

Linux組み込みデバイス向けモノレポ。ドライバ・共有ライブラリ・CLIツールを一元管理する。

> **学習用プロジェクトとして**  
> 本プロジェクトは、実案件レベルの組み込みSW開発フローを体験・学習するための例として構築されています。  
> 仕様書→設計→実装→テスト→CI/CDという開発ライフサイクル全体が一つのリポジトリで確認できます。  
> まずは [学習ガイド](docs/00_project/learning-guide.md) をお読みください。

---

## コンポーネント

| コンポーネント | ディレクトリ | 説明 |
|---|---|---|
| spi-driver | `driver/` | Linux spidev を介した SPI フルデュプレクス通信ドライバ |
| libdevice | `lib/` | ユーザ空間向け共有ライブラリ（PIMPL・非同期API） |
| device-ctl | `cli/` | デバイス操作CLIツール（同期・非同期モード） |

---

## このプロジェクトで学べるC++パターン

| パターン | 場所 | 概要 |
|---|---|---|
| **RAII** | `driver/src/spi_driver.cpp` | デストラクタで fd を自動 close |
| **PIMPL イディオム** | `lib/src/device.cpp` | `Device::Impl` で実装を隠蔽し ABI を安定化 |
| **インターフェース分離** | `driver/include/ispi_driver.hpp` | 純粋仮想クラス `ISpiDriver` で実機とモックを交換可能に |
| **依存注入 (DI)** | `lib/include/device.hpp` | テスト用コンストラクタで `ISpiDriver*` を外部注入 |
| **`[[nodiscard]]` / `noexcept`** | `driver/include/ispi_driver.hpp` | 戻り値無視の防止と例外を使わないエラー設計 |
| **ロガーマクロ** | `driver/include/logger.hpp` | DEBUGビルドは stderr+syslog、RELEASEは syslog のみ |
| **バージョン自動生成** | `driver/include/version.hpp.in` | CMake `configure_file` + `git describe` でビルド情報を埋め込み |
| **Doxygen スニペット** | `driver/include/ispi_driver.hpp` | `@snippet` でテストコードをAPIドキュメントに引用 |
| **GMock** | `tests/mocks/mock_spi_driver.hpp` | `MOCK_METHOD` で実機不要のユニットテスト |

詳しい解説は [学習ガイド](docs/00_project/learning-guide.md) を参照してください。

---

## ドキュメント読み進めガイド

ドキュメントは開発フローの順番に番号が振られています。以下の順で読むと体系的に学べます。

```
01_requirements/   → 何を作るかを決める（要件定義書）
02_basic-design/   → どう作るかの全体像（システム構成・アーキテクチャ）
03_detailed-design/→ クラス設計の詳細（SpiDriver・Device）
04_api-spec/       → 公開APIの仕様書（使う側の視点）
05_interface-spec/ → ハードウェアとのインターフェース仕様
06_test/           → テスト計画・仕様書
07_delivery/       → リリースノート・納品物チェックリスト
```

コードを読む場合のお勧め順序:
1. `driver/include/ispi_driver.hpp` — インターフェース設計
2. `driver/include/spi_driver.hpp` / `driver/src/spi_driver.cpp` — 実機ドライバ実装
3. `lib/include/device.hpp` / `lib/src/device.cpp` — PIMPL + 非同期API
4. `tests/mocks/mock_spi_driver.hpp` — モック実装
5. `tests/unit/` — ユニットテスト

---

## ディレクトリ構成

```
.
├── driver/          # SPIドライバ（静的ライブラリ）
│   ├── include/     #   公開ヘッダ（ISpiDriver, SpiDriver, logger, version）
│   └── src/         #   実装
├── lib/             # libdevice（動的共有ライブラリ）
│   ├── include/     #   公開ヘッダ（Device）
│   └── src/         #   実装
├── cli/             # device-ctl（CLIツール）
│   └── src/         #   main.cpp
├── tests/
│   ├── mocks/       #   MockSpiDriver（テスト用）
│   ├── unit/        #   単体テスト（driver / lib）
│   └── integration/ #   結合テスト（実機必須）
├── docs/            # プロジェクトドキュメント一式
│   ├── 00_project/  #   学習ガイド・議事録・WBS
│   ├── 01_requirements/ → 07_delivery/
│   └── doxygen/     #   Doxygen 生成ドキュメント（自動生成）
├── tools/           # ドキュメント生成スクリプト（gen_docs.py）
├── .github/         # GitHub Actions CI
├── .gitlab-ci.yml   # GitLab CI/CD
├── Doxyfile         # Doxygen 設定
├── Dockerfile.build # Docker ビルド環境
└── CMakeLists.txt   # トップレベル CMake
```

---

## ビルド要件

| ツール | バージョン |
|---|---|
| GCC | 7.5 以上（C++17対応） |
| CMake | 3.10 以上 |
| Google Test | 1.14（単体テスト用） |
| Doxygen | 1.9 以上（ドキュメント生成用） |
| Python 3 + openpyxl | （gen_docs.py 使用時のみ） |

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
# driver 単体テスト
cmake -S driver -B build/driver -DCMAKE_BUILD_TYPE=Debug
cmake --build build/driver
cmake -S tests/unit/driver -B build/test-driver
cmake --build build/test-driver
./build/test-driver/test_spi_driver

# lib 単体テスト
cmake -S lib -B build/lib -DCMAKE_BUILD_TYPE=Debug
cmake --build build/lib
cmake -S tests/unit/lib -B build/test-lib
cmake --build build/test-lib
./build/test-lib/test_device
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
         driver/src/ lib/src/ cli/src/
```

---

## タグ命名規則

```
<component>/v<MAJOR>.<MINOR>.<PATCH>

例:
  driver/v1.0.0
  lib/v1.0.0
  cli/v1.0.0
```

タグがないと `--version` が `v0.0.0-unknown` を表示するため、  
リリース時は必ずタグを作成すること（詳細は `CONTRIBUTING.md` 参照）。

---

## ドキュメント

- [学習ガイド](docs/00_project/learning-guide.md)
- [GitLab移行・運用ガイド](docs/gitlab-migration-guide.md)
- [リリースマトリックス](docs/wiki/release-matrix.md)
- [API仕様書 — libdevice](docs/04_api-spec/libdevice-api.md)
- [API仕様書 — SpiDriver](docs/04_api-spec/spi-driver-api.md)
