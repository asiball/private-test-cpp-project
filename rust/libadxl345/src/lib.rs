//! ADXL345 3軸加速度センサードライバ (SPI)。
//!
//! C++ 対応:
//!   `class Adxl345`      → `Adxl345` 構造体
//!   `adxl345_reg.hpp`    → `reg` モジュール (定数)
//!
//! 設計:
//!   SPI MODE 3, 4MHz, 8bit
//!   open() 時に DEVID(0xE5) を検証してから ±16g / フル解像度モードで初期化。

mod reg;

use reg::*;
use spi_hal::{LinuxSpiDriver, SpiConfig, SpiDriver, SpiError};
use thiserror::Error;

pub const SCALE_G_PER_LSB: f64 = 0.0039; // 3.9 mg/LSB (フル解像度モード)

#[derive(Debug, Error)]
pub enum Adxl345Error {
    #[error("SPI エラー: {0}")]
    Spi(#[from] SpiError),

    #[error("デバイスID が一致しません: 期待=0xE5, 実際=0x{0:02X}")]
    WrongDeviceId(u8),

    #[error("デバイスが開かれていません")]
    NotOpen,
}

/// 生の ADC 値 (符号付き 16 ビット 3 軸)。
#[derive(Debug, Clone, Copy)]
pub struct AccelRaw {
    pub x: i16,
    pub y: i16,
    pub z: i16,
}

/// g 単位に変換した 3 軸加速度。
#[derive(Debug, Clone, Copy)]
pub struct AccelG {
    pub x: f64,
    pub y: f64,
    pub z: f64,
}

/// ADXL345 高水準ドライバ。
pub struct Adxl345 {
    driver: Box<dyn SpiDriver>,
    open: bool,
}

impl Adxl345 {
    pub fn new(spi_path: &str) -> Self {
        Self::with_driver(Box::new(LinuxSpiDriver::new(spi_path)))
    }

    pub fn with_driver(driver: Box<dyn SpiDriver>) -> Self {
        Self { driver, open: false }
    }

    /// デバイスを開いて ID 検証・初期設定を行う。
    ///
    /// Bug #5 fix: `self.open = true` は全初期化が完了した後にのみセットする。
    /// SPI オープン後に DEVID 読み取りや設定書き込みが失敗した場合は
    /// `self.open` を `false` のまま維持し、ドライバを閉じて返る。
    pub fn open(&mut self) -> Result<(), Adxl345Error> {
        let cfg = SpiConfig { speed_hz: 4_000_000, bits_per_word: 8, mode: 3 };
        self.driver.open(&cfg)?;
        // self.open = true はここでは設定しない

        // DEVID を確認 (0xE5 でなければエラー)
        // read_reg は self.open を参照するため、直接 transfer を呼ぶ
        let id = {
            let tx = [DEVID | 0x80, 0x00];
            let mut rx = [0u8; 2];
            self.driver.transfer(&tx, &mut rx).map_err(|e| {
                self.driver.close();
                Adxl345Error::Spi(e)
            })?;
            rx[1]
        };

        if id != 0xE5 {
            self.driver.close();
            return Err(Adxl345Error::WrongDeviceId(id));
        }

        // 初期化: ±16g フル解像度モード設定
        let write_init = |driver: &mut Box<dyn SpiDriver>, addr: u8, value: u8| {
            let tx = [addr & 0x7F, value];
            let mut rx = [0u8; 2];
            driver.transfer(&tx, &mut rx).map_err(Adxl345Error::Spi)
        };

        if let Err(e) = write_init(&mut self.driver, DATA_FORMAT, FULL_RES | RANGE_16G) {
            self.driver.close();
            return Err(e);
        }
        if let Err(e) = write_init(&mut self.driver, POWER_CTL, MEASURE) {
            self.driver.close();
            return Err(e);
        }

        // 全初期化完了後に open フラグをセット
        self.open = true;
        log::debug!("ADXL345 initialized");
        Ok(())
    }

    pub fn close(&mut self) {
        if self.open {
            // スタンバイモードへ
            let _ = self.write_reg(POWER_CTL, 0);
            self.driver.close();
            self.open = false;
        }
    }

    pub fn is_open(&self) -> bool {
        self.open
    }

    /// レジスタを 1 バイト読み出し。
    #[must_use]
    pub fn read_reg(&mut self, addr: u8) -> Result<Option<u8>, Adxl345Error> {
        if !self.open {
            return Ok(None);
        }
        // ADXL345 SPI 読み出し: アドレスバイトの bit7=1 (READ フラグ)
        let tx = [addr | 0x80, 0x00];
        let mut rx = [0u8; 2];
        self.driver.transfer(&tx, &mut rx)?;
        Ok(Some(rx[1]))
    }

    /// レジスタに 1 バイト書き込み。
    pub fn write_reg(&mut self, addr: u8, value: u8) -> Result<(), Adxl345Error> {
        if !self.open {
            return Err(Adxl345Error::NotOpen);
        }
        let tx = [addr & 0x7F, value]; // bit7=0 (WRITE)
        let mut rx = [0u8; 2];
        self.driver.transfer(&tx, &mut rx)?;
        Ok(())
    }

    /// read-modify-write: 指定ビットマスクの範囲だけ更新。
    pub fn update_bits(&mut self, addr: u8, mask: u8, value: u8) -> Result<(), Adxl345Error> {
        let current = self.read_reg(addr)?.ok_or(Adxl345Error::NotOpen)?;
        let updated = (current & !mask) | (value & mask);
        self.write_reg(addr, updated)
    }

    /// 生の加速度値を読み出す (6バイトバーストリード)。
    #[must_use]
    pub fn read_raw(&mut self) -> Result<AccelRaw, Adxl345Error> {
        if !self.open {
            return Err(Adxl345Error::NotOpen);
        }
        // ADXL345 マルチバイト読み出し: bit7=READ, bit6=MB (multi-byte)
        let tx = [DATAX0 | 0xC0, 0, 0, 0, 0, 0, 0];
        let mut rx = [0u8; 7];
        self.driver.transfer(&tx, &mut rx)?;

        // リトルエンディアンで組み立て
        let x = i16::from_le_bytes([rx[1], rx[2]]);
        let y = i16::from_le_bytes([rx[3], rx[4]]);
        let z = i16::from_le_bytes([rx[5], rx[6]]);
        Ok(AccelRaw { x, y, z })
    }

    /// g 単位に変換した加速度値を返す。
    #[must_use]
    pub fn read_g(&mut self) -> Result<AccelG, Adxl345Error> {
        let raw = self.read_raw()?;
        Ok(AccelG {
            x: raw.x as f64 * SCALE_G_PER_LSB,
            y: raw.y as f64 * SCALE_G_PER_LSB,
            z: raw.z as f64 * SCALE_G_PER_LSB,
        })
    }
}

impl Drop for Adxl345 {
    fn drop(&mut self) {
        self.close();
    }
}
