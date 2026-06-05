//! センサーライブラリ (MCP3008 SPI ADC + ADS1115 I2C ADC)
//!
//! C++ 対応:
//!   `class Sensor`   → `Mcp3008` 構造体
//!   `class Ads1115`  → `Ads1115` 構造体
//!
//! PIMPL の代替: Rust の pub(crate)/private フィールドで実装詳細を隠蔽。
//! DI: `Box<dyn SpiDriver>` / `Box<dyn I2cDriver>` を受け取るコンストラクタ。

pub mod ads1115;
pub mod mcp3008;

pub use ads1115::{Ads1115, Gain};
pub use mcp3008::Mcp3008;
