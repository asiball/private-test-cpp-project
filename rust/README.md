# Rust 置き換え探索 (`rust/`)

このディレクトリは、本リポジトリ (`embedded-device-suite`) の C++17 実装を
Rust で書き直した場合にどうなるかを検証するための **探索ブランチ用コード** です。

本番投入を前提としたものではなく、移行コスト・設計対応・学習資料として活用することを目的としています。

---

## ディレクトリ構成

```
rust/
├── Cargo.toml          # Cargo ワークスペース定義
├── common/             # ロガー初期化 (C++: common/include/logger.hpp)
├── spi-hal/            # SPI ドライバ抽象 + Linux spidev 実装
│   └── tests/          # ユニットテスト (実機ケースは自動スキップ)
├── i2c-hal/            # I2C ドライバ抽象 + Linux i2c-dev 実装
│   └── tests/          # ユニットテスト (実機ケースは自動スキップ)
├── gpio/               # GPIO エッジ検知 (Linux GPIO chardev v2)
│   └── tests/          # ユニットテスト + src/lib.rs 内の uABI 回帰テスト
├── libsensor/          # MCP3008 SPI ADC + ADS1115 I2C ADC
│   └── tests/          # ユニットテスト (モック注入)
├── libadxl345/         # ADXL345 3軸加速度センサー
│   └── tests/          # ユニットテスト (モック注入)
└── cli/                # device-ctl インタラクティブ CLI
```

---

## ビルドとテスト

```sh
# ワークスペースごとビルド
cd rust
cargo build

# 全テスト実行 (実機不要 — モック注入)
cargo test

# リリースビルド
cargo build --release

# 静的解析 (Clippy — cppcheck/clang-tidy 相当)
cargo clippy -- -D warnings
```

---

## C++ との設計対応一覧

| C++ の概念 | Rust での置き換え | ファイル |
|---|---|---|
| 純粋仮想クラス `ISpiDriver` | `SpiDriver` **トレイト** | `spi-hal/src/lib.rs` |
| 純粋仮想クラス `II2cDriver` | `I2cDriver` **トレイト** | `i2c-hal/src/lib.rs` |
| PIMPL (`struct Impl` + `unique_ptr`) | **private フィールド** (モジュール境界で隠蔽) | 不要になる |
| DI (`ISpiDriver*` 注入) | `Box<dyn SpiDriver>` | `libsensor/src/mcp3008.rs` |
| `[[nodiscard]]` | `#[must_use]` | 各ドライバ |
| `std::optional<T>` | `Option<T>` | `libadxl345/src/lib.rs` |
| `noexcept` + errno 戻り値 | `Result<T, E>` (thiserror) | 全コンポーネント |
| デストラクタ (RAII) | `Drop` トレイト | 全ドライバ |
| `std::thread` + `condition_variable` | `std::thread` + `Arc<(Mutex<bool>, Condvar)>` | `cli/src/main.rs` |
| `enum class Gain` | Rust `enum Gain` | `libsensor/src/ads1115.rs` |
| `#define` / `constexpr` 定数 | `pub const` | `libadxl345/src/reg.rs` |
| `write_all` の保証がない `write()` | `Write::write_all()` | `i2c-hal/src/lib.rs` |
| I2C Repeated Start (2 トランザクション) | `I2C_RDWR` ioctl (1 アトミックトランザクション) | `i2c-hal/src/lib.rs` |

---

## テスト (C++ 側との対応)

C++ 側のユニットテスト (`tests/unit/*`) と同じ観点を `cargo test` でカバーする。
実機が必要なケースは、デバイスファイルが存在しなければスキップする
(C++ の `GTEST_SKIP()` と同じ方針)。

| Rust テスト | 対応する C++ テスト | 備考 |
|---|---|---|
| `spi-hal/tests/spi_driver_test.rs` | `tests/unit/spi-hal/test_spi_driver.cpp` (UT-DRV-001〜007) | コピー禁止は型システムが保証するため実行時テスト不要 |
| `i2c-hal/tests/i2c_driver_test.rs` | `tests/unit/i2c-hal/test_i2c_driver.cpp` (UT-I2C-001〜007) | 〃 |
| `gpio/tests/gpio_line_test.rs` | `tests/unit/gpio/test_gpio_line.cpp` (UT-GPIO-001〜005) | 〃 |
| `gpio/src/lib.rs` 内 `uabi_tests` | (C++ はカーネルヘッダの定数を直接使うため不要) | 手書きの uABI 定数/レイアウトの回帰テスト |
| `libsensor/tests/mcp3008_test.rs` | `tests/unit/libsensor/test_sensor.cpp` (UT-LIB-001〜009) | モック注入 |
| `libadxl345/tests/adxl345_test.rs` | `tests/unit/libadxl345/test_adxl345.cpp` (UT-ADXL-001〜011) | モック注入 |

### 探索版としての意図的な簡略化

- **ADS1115**: ドライバ (`libsensor/src/ads1115.rs`) は移植済みだが、専用テスト
  (C++ の UT-ADS-001〜007 相当) と CLI からの利用は未対応。
- **`read_raw_async`**: C++ の非同期読み出し API は未移植 (CLI も同期読み出しのみ)。
- **SBOM**: `tools/sbom-metadata.json` の対象外 (リリース成果物ではないため)。

## 学習ガイド

移行コスト・メリット・デメリットの詳細は
[`docs/guides/rust-migration-guide.md`](../docs/guides/rust-migration-guide.md) を参照してください。
