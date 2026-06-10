//! ADS1115 16ビット 4チャンネル I2C ADC ドライバ。
//!
//! C++ `Ads1115` クラス + `ads1115.cpp` 実装に相当。

use i2c_hal::{I2cDriver, I2cError, LinuxI2cDriver};
use thiserror::Error;

/// デフォルト I2C スレーブアドレス (ADDR ピン GND 接続時)。
pub const DEFAULT_ADDR: u16 = 0x48;
/// チャンネル数。
pub const CHANNEL_COUNT: u8 = 4;

// ADS1115 レジスタアドレス
const REG_CONVERSION: u8 = 0x00;
const REG_CONFIG: u8 = 0x01;
const REG_LO_THRESH: u8 = 0x02;
const REG_HI_THRESH: u8 = 0x03;

// Config レジスタビットフィールド
const OS_SINGLE: u16 = 0x8000;
const MUX_CH0_GND: u16 = 0x4000;
const MODE_SINGLE: u16 = 0x0100;
const DR_128SPS: u16 = 0x0080;
const COMP_QUE_DISABLE: u16 = 0x0003;
// 1 変換ごとに ALERT/RDY をアサートする (RDY ピンモード用)。C++ 版 CFG_COMP_QUE_ONE に相当。
const COMP_QUE_ONE: u16 = 0x0000;

// 変換完了待ちポーリングの最大試行回数
// 128 SPS では 1 変換 ≈ 7.8 ms。1 ms ポーリングで 100 回 = 100 ms まで待つ。
const MAX_POLL_RETRIES: u32 = 100;

// ALERT/RDY ピン設定値
const LO_THRESH_CONV_RDY: u16 = 0x0000;
const HI_THRESH_CONV_RDY: u16 = 0x8000;

/// ADS1115 ADC ドライバのエラー型。
#[derive(Debug, Error)]
pub enum Ads1115Error {
    /// 下位 I2C 転送エラー。
    #[error("I2C エラー: {0}")]
    I2c(#[from] I2cError),

    /// 指定チャンネルが 0–3 の範囲外。
    #[error("チャンネル番号が範囲外です: {0} (0–3)")]
    InvalidChannel(u8),

    /// `open()` 前に読み出しを試みた。
    #[error("デバイスが開かれていません")]
    NotOpen,

    /// 変換完了ポーリングが上限回数 (`MAX_POLL_RETRIES`) を超えた。
    #[error("変換完了タイムアウト ({MAX_POLL_RETRIES}ms 経過)")]
    Timeout,
}

/// PGA (プログラマブルゲインアンプ) 設定。C++ `Ads1115::Gain` enum に相当。
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
#[repr(u16)]
pub enum Gain {
    /// フルスケール ±6.144 V。
    Fsr6V144 = 0x0000,
    /// フルスケール ±4.096 V。
    Fsr4V096 = 0x0200,
    /// フルスケール ±2.048 V (デフォルト)。
    #[default]
    Fsr2V048 = 0x0400,
    /// フルスケール ±1.024 V。
    Fsr1V024 = 0x0600,
    /// フルスケール ±0.512 V。
    Fsr0V512 = 0x0800,
    /// フルスケール ±0.256 V。
    Fsr0V256 = 0x0A00,
}

impl Gain {
    /// フルスケール電圧 \[V\] を返す。
    ///
    /// # Examples
    ///
    /// ```
    /// use libsensor::Gain;
    ///
    /// assert!((Gain::Fsr2V048.full_scale_volts() - 2.048).abs() < f64::EPSILON);
    /// assert!((Gain::Fsr6V144.full_scale_volts() - 6.144).abs() < f64::EPSILON);
    /// ```
    #[must_use]
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

/// ADS1115 (4ch / 16bit I2C ADC) 高水準ドライバ。
///
/// 生産コードは [`new`](Self::new)（内部で `LinuxI2cDriver` を生成）、
/// テストは [`with_driver`](Self::with_driver) でモックを注入する（依存注入）。
///
/// - 入力レンジ: PGA（[`Gain`]）に依存。デフォルト ±2.048V
/// - 分解能: 16 bit（符号付き）
/// - 4 チャンネル（A0〜A3、シングルエンド）
///
/// 読み出しはシングルショット変換で、変換完了を I2C ポーリングで待つ。
/// 割り込み駆動にしたい場合は [`enable_conversion_ready_pin`](Self::enable_conversion_ready_pin)
/// と GPIO エッジ検知（`embedded-gpio` クレート）を組み合わせる。
///
/// # Examples
///
/// ```no_run
/// use libsensor::{Ads1115, Gain};
///
/// fn main() -> Result<(), Box<dyn std::error::Error>> {
///     let mut adc = Ads1115::new("/dev/i2c-1", libsensor::ads1115::DEFAULT_ADDR);
///     adc.open()?;
///     adc.set_gain(Gain::Fsr4V096); // 入力レンジ ±4.096V
///     println!("A0 = {:.4} V", adc.read_voltage(0)?);
///     Ok(())
/// } // adc は Drop で自動クローズされる
/// ```
pub struct Ads1115 {
    driver: Box<dyn I2cDriver>,
    addr: u16,
    gain: Gain,
    open: bool,
    rdy_pin_enabled: bool,
}

impl Ads1115 {
    /// 生産環境用: I2C バスパスとスレーブアドレスを受け取る。
    pub fn new(i2c_path: &str, addr: u16) -> Self {
        Self::with_driver(Box::new(LinuxI2cDriver::new(i2c_path)), addr)
    }

    /// テスト / DI 用: `I2cDriver` 実装を直接受け取る。
    pub fn with_driver(driver: Box<dyn I2cDriver>, addr: u16) -> Self {
        Self {
            driver,
            addr,
            gain: Gain::default(),
            open: false,
            rdy_pin_enabled: false,
        }
    }

    /// I2C バスを開き、スレーブアドレスを設定する。
    ///
    /// # Errors
    ///
    /// [`Ads1115Error::I2c`] — バスを開けない、またはアドレス設定に失敗
    pub fn open(&mut self) -> Result<(), Ads1115Error> {
        self.driver.open(self.addr)?;
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

    /// PGA ゲインを設定する。
    pub fn set_gain(&mut self, gain: Gain) {
        self.gain = gain;
    }

    /// 現在の PGA ゲインを返す。
    #[must_use]
    pub fn gain(&self) -> Gain {
        self.gain
    }

    /// 現在のゲイン設定のフルスケール電圧 \[V\] を返す。
    #[must_use]
    pub fn full_scale_volts(&self) -> f64 {
        self.gain.full_scale_volts()
    }

    /// 指定チャンネルをシングルショット変換し、生値（符号付き 16bit）を読む。
    ///
    /// Config レジスタへの書き込みで変換を開始し、OS ビットが 1 に戻るまで
    /// 1ms 間隔で最大 100 回ポーリングしてから Conversion レジスタを読む
    /// （128SPS の変換時間 ≈ 7.8ms に対して十分な余裕を持たせている）。
    ///
    /// # Errors
    ///
    /// - [`Ads1115Error::NotOpen`] — `open()` 前に呼んだ
    /// - [`Ads1115Error::InvalidChannel`] — `channel` が 0〜3 の範囲外
    /// - [`Ads1115Error::Timeout`] — ポーリング上限内に変換が完了しなかった
    /// - [`Ads1115Error::I2c`] — I2C 転送に失敗
    pub fn read_raw(&mut self, channel: u8) -> Result<i16, Ads1115Error> {
        if !self.open {
            return Err(Ads1115Error::NotOpen);
        }
        if channel >= CHANNEL_COUNT {
            return Err(Ads1115Error::InvalidChannel(channel));
        }

        let mux = MUX_CH0_GND | ((channel as u16) << 12);
        // RDY ピン有効時は COMP_QUE=00 (1 変換ごとにアサート) を維持する。
        // COMP_QUE_DISABLE を書くと ALERT/RDY 出力が無効化されてしまう (C++ 版と同じ挙動)。
        let comp_que = if self.rdy_pin_enabled {
            COMP_QUE_ONE
        } else {
            COMP_QUE_DISABLE
        };
        let config: u16 = OS_SINGLE | mux | self.gain as u16 | MODE_SINGLE | DR_128SPS | comp_que;

        self.write_reg(REG_CONFIG, config)?;

        for _ in 0..MAX_POLL_RETRIES {
            let cfg = self.read_reg(REG_CONFIG)?;
            if cfg & 0x8000 != 0 {
                let raw = self.read_reg(REG_CONVERSION)? as i16;
                return Ok(raw);
            }
            std::thread::sleep(std::time::Duration::from_millis(1));
        }
        Err(Ads1115Error::Timeout)
    }

    /// 指定チャンネルの電圧 \[V\] を読む。
    ///
    /// `raw / 32768 * full_scale_volts()` で換算する。換算結果は
    /// [`set_gain`](Self::set_gain) の影響を受ける。
    ///
    /// # Errors
    ///
    /// [`read_raw`](Self::read_raw) と同じ。
    pub fn read_voltage(&mut self, channel: u8) -> Result<f64, Ads1115Error> {
        let raw = self.read_raw(channel)?;
        let vfs = self.gain.full_scale_volts();
        Ok(raw as f64 / 32768.0 * vfs)
    }

    /// ALERT/RDY ピンを「変換完了通知 (RDY)」として有効化する。
    ///
    /// Hi_thresh の MSB=1 / Lo_thresh の MSB=0 を書き込み、以降の変換完了時に
    /// ALERT/RDY ピンがアサートされるよう構成する。有効化後の
    /// [`read_raw`](Self::read_raw) は COMP_QUE を「1 変換ごとにアサート」へ
    /// 切り替えて変換を開始する。GPIO エッジ検知と組み合わせると
    /// ポーリングの代わりに割り込みで変換完了を待てる。
    ///
    /// # Errors
    ///
    /// - [`Ads1115Error::NotOpen`] — `open()` 前に呼んだ
    /// - [`Ads1115Error::I2c`] — 閾値レジスタの書き込みに失敗
    pub fn enable_conversion_ready_pin(&mut self) -> Result<(), Ads1115Error> {
        if !self.open {
            return Err(Ads1115Error::NotOpen);
        }
        self.write_reg(REG_LO_THRESH, LO_THRESH_CONV_RDY)?;
        self.write_reg(REG_HI_THRESH, HI_THRESH_CONV_RDY)?;
        self.rdy_pin_enabled = true;
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
