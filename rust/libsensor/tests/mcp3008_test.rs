//! MCP3008 ユニットテスト。
//!
//! C++ `tests/unit/libsensor/test_sensor.cpp` に相当。
//! `MockSpiDriver` をインジェクトして実機なしでテスト。

use libsensor::Mcp3008;
use spi_hal::{SpiConfig, SpiDriver, SpiError};
use std::collections::VecDeque;
use std::sync::{Arc, Mutex};

/// テスト用モック SPI ドライバ。
/// C++ `MockSpiDriver` (GTest Mock) に相当する Rust 実装。
///
/// `recorded_tx` は `Arc<Mutex<_>>` 経由でテスト側からも参照できる (Bug #10 fix)。
struct MockSpiDriver {
    responses: VecDeque<Vec<u8>>,
    recorded_tx: Arc<Mutex<Vec<Vec<u8>>>>,
    is_open: bool,
}

impl MockSpiDriver {
    /// モックと TX ログへの共有ハンドルを返す。
    #[allow(clippy::type_complexity)]
    fn new() -> (Self, Arc<Mutex<Vec<Vec<u8>>>>) {
        let recorded_tx = Arc::new(Mutex::new(Vec::new()));
        let mock = Self {
            responses: VecDeque::new(),
            recorded_tx: Arc::clone(&recorded_tx),
            is_open: false,
        };
        (mock, recorded_tx)
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
        self.recorded_tx.lock().unwrap().push(tx.to_vec());
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
    let (mock, _tx_log) = MockSpiDriver::new();
    let mut sensor = Mcp3008::with_driver(Box::new(mock), 3.3);

    assert!(!sensor.is_open());
    sensor.open().expect("open に失敗");
    assert!(sensor.is_open());
    sensor.close();
    assert!(!sensor.is_open());
}

/// Bug #1 fix の検証: tx[0]=0x01 (START_BIT) であることを確認。
#[test]
fn test_read_raw_tx_frame_ch0() {
    let (mut mock, tx_log) = MockSpiDriver::new();
    // rx[1] bit1-0 = 0x01, rx[2] = 0xFF → raw = (1<<8)|0xFF = 511
    mock.push_response(vec![0x00, 0x01, 0xFF]);

    let mut sensor = Mcp3008::with_driver(Box::new(mock), 3.3);
    sensor.open().unwrap();
    let raw = sensor.read_raw(0).expect("read_raw に失敗");

    assert_eq!(raw, 511);

    // TX フレームの検証:
    //   tx[0] = 0x01 (START_BIT — Bug #1 で 0x00 だったものが修正済み)
    //   tx[1] = (SINGLE_ENDED=0x08 | ch=0) << 4 = 0x80
    //   tx[2] = 0x00 (ダミー)
    let txs = tx_log.lock().unwrap();
    // open() 時の transfer はないので txs[0] が read_raw の転送
    assert_eq!(txs[0], vec![0x01, 0x80, 0x00], "CH0 tx フレームが不正");
}

#[test]
fn test_read_raw_tx_frame_ch3() {
    let (mut mock, tx_log) = MockSpiDriver::new();
    mock.push_response(vec![0x00, 0x00, 0x00]);

    let mut sensor = Mcp3008::with_driver(Box::new(mock), 3.3);
    sensor.open().unwrap();
    let _ = sensor.read_raw(3).unwrap();

    // CH3: (0x08 | 0x03) << 4 = 0xB0
    let txs = tx_log.lock().unwrap();
    assert_eq!(txs[0], vec![0x01, 0xB0, 0x00], "CH3 tx フレームが不正");
}

#[test]
fn test_read_voltage_converts_correctly() {
    let (mut mock, _) = MockSpiDriver::new();
    // rx[1] bit1-0=0x03, rx[2]=0xFF → raw = 1023 = ADC_MAX → vref=3.3V
    mock.push_response(vec![0x00, 0x03, 0xFF]);

    let mut sensor = Mcp3008::with_driver(Box::new(mock), 3.3);
    sensor.open().unwrap();
    let v = sensor.read_voltage(0).expect("read_voltage に失敗");
    // 1023 / 1023 * 3.3 = 3.3
    assert!((v - 3.3).abs() < 1e-6, "電圧変換が不正: {}", v);
}

#[test]
fn test_invalid_channel_returns_error() {
    let (mock, _) = MockSpiDriver::new();
    let mut sensor = Mcp3008::with_driver(Box::new(mock), 3.3);
    sensor.open().unwrap();
    let result = sensor.read_raw(8); // チャンネル8は範囲外
    assert!(result.is_err());
}

#[test]
fn test_read_without_open_returns_error() {
    let (mock, _) = MockSpiDriver::new();
    let mut sensor = Mcp3008::with_driver(Box::new(mock), 3.3);
    let result = sensor.read_raw(0);
    assert!(result.is_err());
}

#[test]
fn test_vref_setter() {
    let (mock, _) = MockSpiDriver::new();
    let mut sensor = Mcp3008::with_driver(Box::new(mock), 3.3);
    assert!((sensor.vref() - 3.3).abs() < 1e-9);
    sensor.set_vref(5.0);
    assert!((sensor.vref() - 5.0).abs() < 1e-9);
}
