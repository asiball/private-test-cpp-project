//! ADXL345 ユニットテスト。
//!
//! C++ `tests/unit/libadxl345/test_adxl345.cpp` に相当。

use libadxl345::{AccelG, AccelRaw, Adxl345, SCALE_G_PER_LSB};
use spi_hal::{SpiConfig, SpiDriver, SpiError};
use std::collections::VecDeque;

struct MockSpiDriver {
    responses: VecDeque<Vec<u8>>,
    pub open: bool,
}

impl MockSpiDriver {
    fn new() -> Self {
        Self { responses: VecDeque::new(), open: false }
    }

    fn push(&mut self, resp: Vec<u8>) {
        self.responses.push_back(resp);
    }
}

impl SpiDriver for MockSpiDriver {
    fn open(&mut self, _: &SpiConfig) -> Result<(), SpiError> {
        self.open = true;
        Ok(())
    }

    fn close(&mut self) {
        self.open = false;
    }

    fn transfer(&mut self, tx: &[u8], rx: &mut [u8]) -> Result<(), SpiError> {
        if let Some(resp) = self.responses.pop_front() {
            let n = rx.len().min(resp.len());
            rx[..n].copy_from_slice(&resp[..n]);
        } else {
            rx.iter_mut().for_each(|b| *b = 0);
        }
        // tx は検証に使わない (ここでは戻り値のみ確認)
        let _ = tx;
        Ok(())
    }

    fn is_open(&self) -> bool {
        self.open
    }
}

fn make_sensor_with_devid_ok() -> Adxl345 {
    let mut mock = MockSpiDriver::new();
    // open() 内の DEVID read → 0xE5 を返す
    mock.push(vec![0x00, 0xE5]);
    // write_reg(DATA_FORMAT), write_reg(POWER_CTL) は応答を消費しないが
    // transfer は呼ばれる → 空レスポンスで OK
    mock.push(vec![0x00, 0x00]);
    mock.push(vec![0x00, 0x00]);
    let mut s = Adxl345::with_driver(Box::new(mock));
    s.open().expect("open に失敗");
    s
}

#[test]
fn test_open_verifies_devid() {
    let _ = make_sensor_with_devid_ok();
}

#[test]
fn test_open_wrong_devid_returns_error() {
    let mut mock = MockSpiDriver::new();
    mock.push(vec![0x00, 0x00]); // DEVID = 0x00 (不正)
    let mut s = Adxl345::with_driver(Box::new(mock));
    let result = s.open();
    assert!(result.is_err(), "不正なデバイスID でエラーになるはず");
}

#[test]
fn test_read_raw_returns_xyz() {
    let mut s = make_sensor_with_devid_ok();

    // 6バイトバーストリード: x=256, y=512, z=-1 (リトルエンディアン)
    // rx[1..2] = x, rx[3..4] = y, rx[5..6] = z
    let x_raw: i16 = 256;
    let y_raw: i16 = 512;
    let z_raw: i16 = -1;
    let mut resp = vec![0u8; 7];
    resp[1..3].copy_from_slice(&x_raw.to_le_bytes());
    resp[3..5].copy_from_slice(&y_raw.to_le_bytes());
    resp[5..7].copy_from_slice(&z_raw.to_le_bytes());

    // MockSpiDriver に追加するためダウンキャストが必要だが、
    // ここでは別の Adxl345 を作り直す
    let mut mock = MockSpiDriver::new();
    mock.push(vec![0x00, 0xE5]); // DEVID
    mock.push(vec![0x00, 0x00]); // DATA_FORMAT write
    mock.push(vec![0x00, 0x00]); // POWER_CTL write
    mock.push(resp);             // read_raw バースト

    let mut s2 = Adxl345::with_driver(Box::new(mock));
    s2.open().unwrap();

    let raw = s2.read_raw().unwrap();
    assert_eq!(raw.x, 256);
    assert_eq!(raw.y, 512);
    assert_eq!(raw.z, -1);
}

#[test]
fn test_read_g_scales_correctly() {
    let x_raw: i16 = 100;
    let mut resp = vec![0u8; 7];
    resp[1..3].copy_from_slice(&x_raw.to_le_bytes());

    let mut mock = MockSpiDriver::new();
    mock.push(vec![0x00, 0xE5]);
    mock.push(vec![0x00, 0x00]);
    mock.push(vec![0x00, 0x00]);
    mock.push(resp);

    let mut s = Adxl345::with_driver(Box::new(mock));
    s.open().unwrap();
    let g = s.read_g().unwrap();
    let expected = 100.0 * SCALE_G_PER_LSB;
    assert!((g.x - expected).abs() < 1e-9, "x軸スケーリングが不正: {}", g.x);
}

#[test]
fn test_read_without_open_returns_error() {
    let mock = MockSpiDriver::new();
    let mut s = Adxl345::with_driver(Box::new(mock));
    assert!(s.read_raw().is_err());
}
