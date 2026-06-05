//! MCP3008 10ビット 8チャンネル SPI ADC ドライバ。
//!
//! C++ `Sensor` クラス + `sensor.cpp` 実装に相当。

use spi_hal::{LinuxSpiDriver, SpiConfig, SpiDriver, SpiError};
use thiserror::Error;

pub const CHANNEL_COUNT: u8 = 8;
pub const ADC_MAX: u16 = 1023;
pub const DEFAULT_VREF: f64 = 3.3;

// MCP3008 SPI 転送フォーマット (データシート Figure 6-1)
//   tx[0] = START_BIT (0x01)
//   tx[1] = SGL/DIF=1 (単一端) + D2..D0 (チャンネル選択) を上位ニブルに配置
//   tx[2] = 0x00 (ダミー、変換完了クロックを供給)
//   rx[1] の bit1-0 が B9,B8、rx[2] が B7..B0
const START_BIT: u8 = 0x01;
const SINGLE_ENDED: u8 = 0x08; // SGL=1: シングルエンドモード

#[derive(Debug, Error)]
pub enum SensorError {
    #[error("SPI エラー: {0}")]
    Spi(#[from] SpiError),

    #[error("チャンネル番号が範囲外です: {0} (0–7)")]
    InvalidChannel(u8),

    #[error("デバイスが開かれていません")]
    NotOpen,
}

/// MCP3008 ADC 高水準ドライバ。
///
/// 生産コード用コンストラクタは `new()` (内部で `LinuxSpiDriver` を生成)。
/// テスト用は `with_driver()` でモックを注入。
/// C++ の PIMPL に相当するフィールド隠蔽は Rust のプライベートフィールドで実現。
pub struct Mcp3008 {
    driver: Box<dyn SpiDriver>,
    vref: f64,
    open: bool,
}

impl Mcp3008 {
    /// 生産環境用: SPI デバイスパスと基準電圧を受け取る。
    pub fn new(spi_path: &str, vref: f64) -> Self {
        Self::with_driver(Box::new(LinuxSpiDriver::new(spi_path)), vref)
    }

    /// テスト / DI 用: `SpiDriver` 実装を直接受け取る。
    pub fn with_driver(driver: Box<dyn SpiDriver>, vref: f64) -> Self {
        Self { driver, vref, open: false }
    }

    pub fn open(&mut self) -> Result<(), SensorError> {
        let cfg = SpiConfig { speed_hz: 1_350_000, bits_per_word: 8, mode: 0 };
        self.driver.open(&cfg)?;
        self.open = true;
        Ok(())
    }

    pub fn close(&mut self) {
        self.driver.close();
        self.open = false;
    }

    pub fn is_open(&self) -> bool {
        self.open
    }

    /// 生の ADC 値を読み出す (0–1023)。
    #[must_use]
    pub fn read_raw(&mut self, channel: u8) -> Result<u16, SensorError> {
        if !self.open {
            return Err(SensorError::NotOpen);
        }
        if channel >= CHANNEL_COUNT {
            return Err(SensorError::InvalidChannel(channel));
        }

        // tx[0]=START_BIT でスタートビットを最初のバイトに配置する (Bug #1 fix)
        // tx[1]: SGL=1, チャンネル番号を D2..D0 として上位ニブルに配置
        let tx = [START_BIT, (SINGLE_ENDED | channel) << 4, 0x00];
        let mut rx = [0u8; 3];
        self.driver.transfer(&tx, &mut rx)?;

        // 10 ビット結果: rx[1] の下位 2 ビット (B9,B8) + rx[2] の全ビット (B7..B0)
        let raw = ((rx[1] as u16 & 0x03) << 8) | rx[2] as u16;
        Ok(raw)
    }

    /// 電圧値に変換して読み出す。
    #[must_use]
    pub fn read_voltage(&mut self, channel: u8) -> Result<f64, SensorError> {
        let raw = self.read_raw(channel)?;
        Ok(raw as f64 / ADC_MAX as f64 * self.vref)
    }

    pub fn vref(&self) -> f64 {
        self.vref
    }

    pub fn set_vref(&mut self, vref: f64) {
        self.vref = vref;
    }
}

impl Drop for Mcp3008 {
    fn drop(&mut self) {
        self.close();
    }
}
