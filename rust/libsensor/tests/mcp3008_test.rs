//! MCP3008 ユニットテスト。
//!
//! C++ `tests/unit/libsensor/test_sensor.cpp` に相当。
//! `MockSpiDriver` をインジェクトして実機なしでテスト。

use libsensor::Mcp3008;
use spi_hal::{SpiConfig, SpiDriver, SpiError};

/// テスト用モック SPI ドライバ。
/// C++ `MockSpiDriver` (GTest Mock) に相当する Rust 実装。
struct MockSpiDriver {
    /// transfer() が返すレスポンスバイト列のキュー
    responses: std::collections::VecDeque<Vec<u8>>,
    /// transfer() に渡された tx バイト列の記録
    pub recorded_tx: Vec<Vec<u8>>,
    pub is_open: bool,
}

impl MockSpiDriver {
    fn new() -> Self {
        Self {
            responses: std::collections::VecDeque::new(),
            recorded_tx: Vec::new(),
            is_open: false,
        }
    }

    fn push_response(&mut self, resp: Vec<u8>) {
        self.responses.push_back(resp);
    }
}

impl SpiDriver for MockSpiDriver {
    fn open(&mut self, _config: &SpiConfig) -> Result<(), SpiError> {
        self.is_open = true;
        Ok(())
    }

    fn close(&mut self) {
        self.is_open = false;
    }

    fn transfer(&mut self, tx: &[u8], rx: &mut [u8]) -> Result<(), SpiError> {
        self.recorded_tx.push(tx.to_vec());
        if let Some(resp) = self.responses.pop_front() {
            let len = rx.len().min(resp.len());
            rx[..len].copy_from_slice(&resp[..len]);
        }
        Ok(())
    }

    fn is_open(&self) -> bool {
        self.is_open
    }
}

#[test]
fn test_open_close() {
    let mock = Box::new(MockSpiDriver::new());
    let mut sensor = Mcp3008::with_driver(mock, 3.3);

    assert!(!sensor.is_open());
    sensor.open().expect("open に失敗");
    assert!(sensor.is_open());
    sensor.close();
    assert!(!sensor.is_open());
}

#[test]
fn test_read_raw_ch0() {
    let mut mock = MockSpiDriver::new();
    // MCP3008 応答: rx[1]のbit1-0 と rx[2] で 10bit 値を構成
    // 例: 0x01FF = 511 (フルスケールの約半分)
    mock.push_response(vec![0x00, 0x01, 0xFF]);

    let mut sensor = Mcp3008::with_driver(Box::new(mock), 3.3);
    sensor.open().unwrap();
    let raw = sensor.read_raw(0).expect("read_raw に失敗");
    // rx[1] & 0x03 = 0x01, rx[2] = 0xFF → (1 << 8) | 255 = 511
    assert_eq!(raw, 511);
}

#[test]
fn test_read_voltage_converts_correctly() {
    let mut mock = MockSpiDriver::new();
    // ADC_MAX = 1023 → フルスケール = vref = 3.3V
    mock.push_response(vec![0x00, 0x03, 0xFF]);

    let mut sensor = Mcp3008::with_driver(Box::new(mock), 3.3);
    sensor.open().unwrap();
    let v = sensor.read_voltage(0).expect("read_voltage に失敗");
    // (1023 / 1023) * 3.3 = 3.3
    assert!((v - 3.3).abs() < 1e-6, "電圧変換が不正: {}", v);
}

#[test]
fn test_invalid_channel_returns_error() {
    let mock = MockSpiDriver::new();
    let mut sensor = Mcp3008::with_driver(Box::new(mock), 3.3);
    sensor.open().unwrap();
    let result = sensor.read_raw(8); // チャンネル8は範囲外
    assert!(result.is_err());
}

#[test]
fn test_read_without_open_returns_error() {
    let mock = MockSpiDriver::new();
    let mut sensor = Mcp3008::with_driver(Box::new(mock), 3.3);
    let result = sensor.read_raw(0);
    assert!(result.is_err());
}

#[test]
fn test_vref_setter() {
    let mock = MockSpiDriver::new();
    let mut sensor = Mcp3008::with_driver(Box::new(mock), 3.3);
    assert!((sensor.vref() - 3.3).abs() < 1e-9);
    sensor.set_vref(5.0);
    assert!((sensor.vref() - 5.0).abs() < 1e-9);
}
