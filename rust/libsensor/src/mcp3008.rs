//! MCP3008 10ビット 8チャンネル SPI ADC ドライバ。
//!
//! C++ `Sensor` クラス + `sensor.cpp` 実装に相当。

use spi_hal::{LinuxSpiDriver, SpiConfig, SpiDriver, SpiError};
use thiserror::Error;

/// チャンネル数。
pub const CHANNEL_COUNT: u8 = 8;
/// ADC の最大生値 (10 ビット)。
pub const ADC_MAX: u16 = 1023;
/// デフォルト基準電圧 \[V\]。
pub const DEFAULT_VREF: f64 = 3.3;

// MCP3008 SPI 転送フォーマット (データシート Figure 6-1)
//   tx[0] = START_BIT (0x01)
//   tx[1] = SGL/DIF=1 (単一端) + D2..D0 (チャンネル選択) を上位ニブルに配置
//   tx[2] = 0x00 (ダミー、変換完了クロックを供給)
//   rx[1] の bit1-0 が B9,B8、rx[2] が B7..B0
const START_BIT: u8 = 0x01;
const SINGLE_ENDED: u8 = 0x08; // SGL=1: シングルエンドモード

/// MCP3008 ADC ドライバのエラー型。
#[derive(Debug, Error)]
pub enum SensorError {
    /// 下位 SPI 転送エラー。
    #[error("SPI エラー: {0}")]
    Spi(#[from] SpiError),

    /// 指定チャンネルが 0–7 の範囲外。
    #[error("チャンネル番号が範囲外です: {0} (0–7)")]
    InvalidChannel(u8),

    /// `open()` 前に読み出しを試みた。
    #[error("デバイスが開かれていません")]
    NotOpen,
}

/// MCP3008 (8ch / 10bit SPI ADC) 高水準ドライバ。
///
/// 生産コードは [`new`](Self::new)（内部で `LinuxSpiDriver` を生成）、
/// テストは [`with_driver`](Self::with_driver) でモックを注入する（依存注入）。
/// C++ の PIMPL に相当するフィールド隠蔽は Rust のプライベートフィールドで実現。
///
/// - 入力レンジ: 0V 〜 vref
/// - 分解能: 10 bit（0〜1023）
/// - 8 チャンネル（CH0〜CH7、シングルエンド）
///
/// # Examples
///
/// ```no_run
/// use libsensor::Mcp3008;
///
/// fn main() -> Result<(), Box<dyn std::error::Error>> {
///     let mut adc = Mcp3008::new("/dev/spidev0.0", 3.3);
///     adc.open()?;
///     println!("CH0 = {:.3} V", adc.read_voltage(0)?);
///     Ok(())
/// } // adc は Drop で自動クローズされる
/// ```
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
        Self {
            driver,
            vref,
            open: false,
        }
    }

    /// デバイスを開き、MCP3008 向けの SPI パラメータを設定する。
    ///
    /// 設定値は 1.35 MHz（Vdd=3.3V 時のデータシート上限）/ 8bit / SPI MODE 0。
    ///
    /// # Errors
    ///
    /// [`SensorError::Spi`] — デバイスを開けない、または ioctl 設定に失敗
    pub fn open(&mut self) -> Result<(), SensorError> {
        let cfg = SpiConfig {
            speed_hz: 1_350_000,
            bits_per_word: 8,
            mode: 0,
        };
        self.driver.open(&cfg)?;
        self.open = true;
        Ok(())
    }

    /// デバイスを閉じる。
    pub fn close(&mut self) {
        self.driver.close();
        self.open = false;
    }

    /// デバイスが開かれているか返す。
    #[must_use]
    pub fn is_open(&self) -> bool {
        self.open
    }

    /// 指定チャンネルの生 ADC 値（0〜1023）をシングルエンドモードで読む。
    ///
    /// 1 回の全二重転送（3 バイト）で開始ビット・チャンネル指定を送り、
    /// 応答の下位 10bit を合成して返す。
    ///
    /// # Errors
    ///
    /// - [`SensorError::NotOpen`] — `open()` 前に呼んだ
    /// - [`SensorError::InvalidChannel`] — `channel` が 0〜7 の範囲外
    /// - [`SensorError::Spi`] — SPI 転送に失敗
    pub fn read_raw(&mut self, channel: u8) -> Result<u16, SensorError> {
        if !self.open {
            return Err(SensorError::NotOpen);
        }
        if channel >= CHANNEL_COUNT {
            return Err(SensorError::InvalidChannel(channel));
        }

        // tx[0]=START_BIT でスタートビットを最初のバイトに配置する
        // tx[1]: SGL=1, チャンネル番号を D2..D0 として上位ニブルに配置
        let tx = [START_BIT, (SINGLE_ENDED | channel) << 4, 0x00];
        let mut rx = [0u8; 3];
        self.driver.transfer(&tx, &mut rx)?;

        // 10 ビット結果: rx[1] の下位 2 ビット (B9,B8) + rx[2] の全ビット (B7..B0)
        let raw = ((rx[1] as u16 & 0x03) << 8) | rx[2] as u16;
        Ok(raw)
    }

    /// 指定チャンネルの電圧 \[V\] を読む。
    ///
    /// `raw * vref / 1023` で換算する。換算結果は [`set_vref`](Self::set_vref) の影響を受ける。
    ///
    /// # Errors
    ///
    /// [`read_raw`](Self::read_raw) と同じ。
    pub fn read_voltage(&mut self, channel: u8) -> Result<f64, SensorError> {
        let raw = self.read_raw(channel)?;
        Ok(raw as f64 / ADC_MAX as f64 * self.vref)
    }

    /// 現在の基準電圧 \[V\] を返す。
    #[must_use]
    pub fn vref(&self) -> f64 {
        self.vref
    }

    /// 基準電圧 \[V\] を変更する。
    pub fn set_vref(&mut self, vref: f64) {
        self.vref = vref;
    }
}

impl Drop for Mcp3008 {
    fn drop(&mut self) {
        self.close();
    }
}
