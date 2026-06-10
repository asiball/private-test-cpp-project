//! LinuxI2cDriver のユニットテスト。
//!
//! C++ 版 `tests/unit/i2c-hal/test_i2c_driver.cpp` (UT-I2C-001〜007) に対応する。
//! 実機 (/dev/i2c-1) が必要なケースはデバイスが無ければスキップする
//! (C++ の GTEST_SKIP() と同じ方針)。

use i2c_hal::{I2cDriver, I2cError, LinuxI2cDriver};

// UT-I2C-001: 無効パスで open() 失敗
#[test]
fn open_invalid_device_fails() {
    let mut drv = LinuxI2cDriver::new("/dev/i2c-99");
    let err = drv.open(0x48).unwrap_err();
    assert!(matches!(err, I2cError::Open(_)));
    assert!(!drv.is_open());
}

// UT-I2C-002: 未オープンで write() するとエラー
#[test]
fn write_not_open_fails() {
    let mut drv = LinuxI2cDriver::new("/dev/i2c-99");
    let err = drv.write(&[0x01]).unwrap_err();
    assert!(matches!(err, I2cError::NotOpen));
}

// UT-I2C-003: 未オープンで read() するとエラー
#[test]
fn read_not_open_fails() {
    let mut drv = LinuxI2cDriver::new("/dev/i2c-99");
    let mut buf = [0u8; 2];
    let err = drv.read(&mut buf).unwrap_err();
    assert!(matches!(err, I2cError::NotOpen));
}

// UT-I2C-004: 未オープンで write_read() するとエラー
#[test]
fn write_read_not_open_fails() {
    let mut drv = LinuxI2cDriver::new("/dev/i2c-99");
    let mut buf = [0u8; 2];
    let err = drv.write_read(&[0x00], &mut buf).unwrap_err();
    assert!(matches!(err, I2cError::NotOpen));
}

// UT-I2C-005: 二重 close は安全
#[test]
fn double_close_is_safe() {
    let mut drv = LinuxI2cDriver::new("/dev/i2c-99");
    drv.close();
    drv.close(); // 2 回呼んでもパニックしない
    assert!(!drv.is_open());
}

// UT-I2C-006 相当: コピー禁止
// Rust では LinuxI2cDriver が Clone/Copy を実装しないことで型システムが保証する。

// UT-I2C-007: 有効バスで open() 成功（実機のみ）
#[test]
fn open_valid_bus_succeeds() {
    let dev = "/dev/i2c-1";
    if !std::path::Path::new(dev).exists() {
        eprintln!("skip: {dev} not available");
        return;
    }
    let mut drv = LinuxI2cDriver::new(dev);
    assert!(drv.open(0x48).is_ok());
    assert!(drv.is_open());

    // C++ 版と同じく、close せずに再 open すると AlreadyOpen で拒否される
    let err = drv.open(0x48).unwrap_err();
    assert!(matches!(err, I2cError::AlreadyOpen));
}

// Rust 固有: I2C メッセージ上限 (65535 バイト) 超過は BufferTooLarge
#[test]
fn write_read_oversized_buffer_fails() {
    let dev = "/dev/i2c-1";
    if !std::path::Path::new(dev).exists() {
        // バッファ長チェックは open 後に行われるため実機が必要
        eprintln!("skip: {dev} not available");
        return;
    }
    let mut drv = LinuxI2cDriver::new(dev);
    assert!(drv.open(0x48).is_ok());
    let tx = vec![0u8; usize::from(u16::MAX) + 1];
    let mut rx = [0u8; 2];
    let err = drv.write_read(&tx, &mut rx).unwrap_err();
    assert!(matches!(err, I2cError::BufferTooLarge(_)));
}
