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
├── i2c-hal/            # I2C ドライバ抽象 + Linux i2c-dev 実装
├── gpio/               # GPIO エッジ検知 (Linux GPIO chardev v2)
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

## 学習ガイド

移行コスト・メリット・デメリットの詳細は
[`docs/guides/rust-migration-guide.md`](../docs/guides/rust-migration-guide.md) を参照してください。
