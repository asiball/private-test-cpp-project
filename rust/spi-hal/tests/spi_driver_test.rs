//! LinuxSpiDriver のユニットテスト。
//!
//! C++ 版 `tests/unit/spi-hal/test_spi_driver.cpp` (UT-DRV-001〜007) に対応する。
//! 実機 (/dev/spidev0.0) が必要なケースはデバイスが無ければスキップする
//! (C++ の GTEST_SKIP() と同じ方針)。

use spi_hal::{LinuxSpiDriver, SpiConfig, SpiDriver, SpiError};

fn default_config() -> SpiConfig {
    SpiConfig {
        speed_hz: 1_000_000,
        bits_per_word: 8,
        mode: 0,
    }
}

// UT-DRV-001: 有効パスで open() 成功（実機のみ）
#[test]
fn open_valid_device_succeeds() {
    let dev = "/dev/spidev0.0";
    if !std::path::Path::new(dev).exists() {
        eprintln!("skip: {dev} not available");
        return;
    }
    let mut drv = LinuxSpiDriver::new(dev);
    assert!(drv.open(&default_config()).is_ok());
    assert!(drv.is_open());
}

// UT-DRV-002: 無効パスで open() 失敗
#[test]
fn open_invalid_device_fails() {
    let mut drv = LinuxSpiDriver::new("/dev/spidevXX.0");
    let err = drv.open(&default_config()).unwrap_err();
    assert!(matches!(err, SpiError::Open(_)));
    assert!(!drv.is_open());
}

// UT-DRV-003 相当: 未オープン時は is_open() が false
#[test]
fn is_open_false_before_open() {
    let drv = LinuxSpiDriver::new("/dev/spidev0.0");
    assert!(!drv.is_open());
}

// UT-DRV-004: 二重 close は安全
#[test]
fn double_close_is_safe() {
    let mut drv = LinuxSpiDriver::new("/dev/spidevXX.0");
    drv.close();
    drv.close(); // 2 回呼んでもパニックしない
    assert!(!drv.is_open());
}

// UT-DRV-005: 未オープンで transfer() するとエラー
#[test]
fn transfer_not_open_fails() {
    let mut drv = LinuxSpiDriver::new("/dev/spidevXX.0");
    let tx = [0u8; 3];
    let mut rx = [0u8; 3];
    let err = drv.transfer(&tx, &mut rx).unwrap_err();
    assert!(matches!(err, SpiError::NotOpen));
}

// Rust 固有: tx/rx 長不一致は LengthMismatch
// (C++ は len を 1 つしか取らないため対応ケースなし)
#[test]
fn transfer_length_mismatch_fails() {
    let mut drv = LinuxSpiDriver::new("/dev/spidevXX.0");
    let tx = [0u8; 3];
    let mut rx = [0u8; 2];
    let err = drv.transfer(&tx, &mut rx).unwrap_err();
    assert!(matches!(err, SpiError::LengthMismatch { tx: 3, rx: 2 }));
}

// UT-DRV-006 相当: コピー禁止
// C++ はコピーコンストラクタ delete を static_assert で確認するが、
// Rust では LinuxSpiDriver が Clone/Copy を実装しないことで型システムが保証する
// (実装すると Drop と Copy は共存できずコンパイルエラー)。実行時テストは不要。

// UT-DRV-007: close せずに二重 open するとエラー（実機のみ）
#[test]
fn double_open_without_close_fails() {
    let dev = "/dev/spidev0.0";
    if !std::path::Path::new(dev).exists() {
        eprintln!("skip: {dev} not available");
        return;
    }
    let mut drv = LinuxSpiDriver::new(dev);
    assert!(drv.open(&default_config()).is_ok());
    // C++ 版と同じく、close せずに再 open すると AlreadyOpen で拒否される
    let err = drv.open(&default_config()).unwrap_err();
    assert!(matches!(err, SpiError::AlreadyOpen));
    assert!(drv.is_open()); // 元のオープン状態は維持される
}
