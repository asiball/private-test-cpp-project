//! ADS1115 16ビット 4チャンネル I2C ADC ドライバ。
//!
//! C++ `Ads1115` クラス + `ads1115.cpp` 実装に相当。

use i2c_hal::{I2cDriver, I2cError, LinuxI2cDriver};
use thiserror::Error;

pub const DEFAULT_ADDR: u16 = 0x48;
pub const CHANNEL_COUNT: u8 = 4;

// ADS1115 レジスタアドレス
const REG_CONVERSION: u8 = 0x00;
const REG_CONFIG: u8 = 0x01;

// Config レジスタビットフィールド
const OS_SINGLE: u16 = 0x8000; // 単発変換開始
const MUX_CH0_GND: u16 = 0x4000; // 入力マルチプレクサ基準

const MODE_SINGLE: u16 = 0x0100; // 単発モード
const DR_128SPS: u16 = 0x0080; // データレート 128 SPS
const COMP_QUE_DISABLE: u16 = 0x0003; // コンパレータ無効

// ALERT/RDY ピンを変換完了通知に使う設定 (Hi/Lo しきい値反転)
const LO_THRESH_CONV_RDY: u16 = 0x0000;
const HI_THRESH_CONV_RDY: u16 = 0x8000;

#[derive(Debug, Error)]
pub enum Ads1115Error {
    #[error("I2C エラー: {0}")]
    I2c(#[from] I2cError),

    #[error("チャンネル番号が範囲外です: {0} (0–3)")]
    InvalidChannel(u8),

    #[error("デバイスが開かれていません")]
    NotOpen,
}

/// PGA (プログラマブルゲインアンプ) 設定。
/// C++ `Ads1115::Gain` enum に相当。
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
#[repr(u16)]
pub enum Gain {
    Fsr6V144 = 0x0000,
    Fsr4V096 = 0x0200,
    #[default]
    Fsr2V048 = 0x0400,
    Fsr1V024 = 0x0600,
    Fsr0V512 = 0x0800,
    Fsr0V256 = 0x0A00,
}

impl Gain {
    /// フルスケール電圧を返す。
    pub fn full_scale_volts(self) -> f64 {
        match self {
            Gain::Fsr6V144 => 6.144,
            Gain::Fsr4V096 => 4.096,
            Gain::Fsr2V048 => 2.048,
            Gain::Fsr1V024 => 1.024,
            Gain::Fsr0V512 => 0.512,
            Gain::Fsr0V256 => 0.256,
        }
    }
}

/// ADS1115 ADC 高水準ドライバ。
pub struct Ads1115 {
    driver: Box<dyn I2cDriver>,
    addr: u16,
    gain: Gain,
    open: bool,
}

impl Ads1115 {
    pub fn new(i2c_path: &str, addr: u16) -> Self {
        Self::with_driver(Box::new(LinuxI2cDriver::new(i2c_path)), addr)
    }

    pub fn with_driver(driver: Box<dyn I2cDriver>, addr: u16) -> Self {
        Self { driver, addr, gain: Gain::default(), open: false }
    }

    pub fn open(&mut self) -> Result<(), Ads1115Error> {
        self.driver.open(self.addr)?;
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

    pub fn set_gain(&mut self, gain: Gain) {
        self.gain = gain;
    }

    pub fn gain(&self) -> Gain {
        self.gain
    }

    pub fn full_scale_volts(&self) -> f64 {
        self.gain.full_scale_volts()
    }

    /// 生の ADC 値を読み出す (符号付き 16 ビット)。
    #[must_use]
    pub fn read_raw(&mut self, channel: u8) -> Result<i16, Ads1115Error> {
        if !self.open {
            return Err(Ads1115Error::NotOpen);
        }
        if channel >= CHANNEL_COUNT {
            return Err(Ads1115Error::InvalidChannel(channel));
        }

        // Config レジスタを構築して単発変換を開始
        let mux = MUX_CH0_GND | ((channel as u16) << 12);
        let config: u16 = OS_SINGLE
            | mux
            | self.gain as u16
            | MODE_SINGLE
            | DR_128SPS
            | COMP_QUE_DISABLE;

        self.write_reg(REG_CONFIG, config)?;

        // 変換完了待機: OS ビットがセットされるまでポーリング
        loop {
            let cfg = self.read_reg(REG_CONFIG)?;
            if cfg & 0x8000 != 0 {
                break;
            }
            std::thread::sleep(std::time::Duration::from_millis(1));
        }

        let raw = self.read_reg(REG_CONVERSION)? as i16;
        Ok(raw)
    }

    /// 電圧値に変換して読み出す。
    #[must_use]
    pub fn read_voltage(&mut self, channel: u8) -> Result<f64, Ads1115Error> {
        let raw = self.read_raw(channel)?;
        let vfs = self.gain.full_scale_volts();
        Ok(raw as f64 / 32768.0 * vfs)
    }

    /// ALERT/RDY ピンを変換完了通知として使う設定。
    /// C++ の `enable_conversion_ready_pin()` に相当。
    pub fn enable_conversion_ready_pin(&mut self) -> Result<(), Ads1115Error> {
        self.write_reg(0x02, LO_THRESH_CONV_RDY)?; // Lo_thresh
        self.write_reg(0x03, HI_THRESH_CONV_RDY)?; // Hi_thresh
        Ok(())
    }

    fn write_reg(&mut self, reg: u8, value: u16) -> Result<(), Ads1115Error> {
        let buf = [reg, (value >> 8) as u8, (value & 0xFF) as u8];
        self.driver.write(&buf)?;
        Ok(())
    }

    fn read_reg(&mut self, reg: u8) -> Result<u16, Ads1115Error> {
        let mut buf = [0u8; 2];
        self.driver.write_read(&[reg], &mut buf)?;
        Ok(((buf[0] as u16) << 8) | buf[1] as u16)
    }
}

impl Drop for Ads1115 {
    fn drop(&mut self) {
        self.close();
    }
}
