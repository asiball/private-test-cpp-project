//! ADXL345 レジスタマップ定数。
//!
//! C++ `adxl345_reg.hpp` に相当。
//! C++ では `#define` や enum class だったものを Rust では `pub const` で定義。

// レジスタアドレス
pub const DEVID: u8 = 0x00;
pub const BW_RATE: u8 = 0x2C;
pub const POWER_CTL: u8 = 0x2D;
pub const DATA_FORMAT: u8 = 0x31;
pub const DATAX0: u8 = 0x32;
pub const DATAX1: u8 = 0x33;
pub const DATAY0: u8 = 0x34;
pub const DATAY1: u8 = 0x35;
pub const DATAZ0: u8 = 0x36;
pub const DATAZ1: u8 = 0x37;

// DATA_FORMAT ビット
pub const FULL_RES: u8 = 0x08; // フル解像度モード
pub const RANGE_2G: u8 = 0x00;
pub const RANGE_4G: u8 = 0x01;
pub const RANGE_8G: u8 = 0x02;
pub const RANGE_16G: u8 = 0x03;

// POWER_CTL ビット
pub const MEASURE: u8 = 0x08; // 測定モード開始

// BW_RATE データレート設定
pub const RATE_100HZ: u8 = 0x0A;
pub const RATE_200HZ: u8 = 0x0B;
pub const RATE_400HZ: u8 = 0x0C;
