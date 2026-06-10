//! GpioLine のユニットテスト。
//!
//! C++ 版 `tests/unit/gpio/test_gpio_line.cpp` (UT-GPIO-001〜005) に対応する。
//! 実機 (/dev/gpiochip0) が必要なケースはデバイスが無ければスキップする
//! (C++ の GTEST_SKIP() と同じ方針)。
//!
//! uABI 定数・構造体レイアウトの回帰テストは `gpio/src/lib.rs` の
//! `uabi_tests` モジュール (cargo test で同時に実行される) を参照。

use embedded_gpio::{Edge, GpioError, GpioLine};

// UT-GPIO-001: request_edge_events() 前に wait_event() するとエラー
#[test]
fn wait_before_request_fails() {
    let mut line = GpioLine::new("/dev/gpiochip0", 0);
    let err = line.wait_event(10).unwrap_err();
    assert!(matches!(err, GpioError::NotRequested));
}

// UT-GPIO-002: 無効チップパスで request_edge_events() 失敗
#[test]
fn request_invalid_chip_fails() {
    let mut line = GpioLine::new("/dev/gpiochip99", 0);
    let err = line.request_edge_events(Edge::Both).unwrap_err();
    assert!(matches!(err, GpioError::Open(_)));
    assert!(!line.is_requested());
}

// UT-GPIO-003: 二重 close は安全
#[test]
fn double_close_is_safe() {
    let mut line = GpioLine::new("/dev/gpiochip99", 0);
    line.close();
    line.close(); // 2 回呼んでもパニックしない
    assert!(!line.is_requested());
}

// UT-GPIO-004 相当: コピー禁止
// Rust では GpioLine が Clone/Copy を実装しないことで型システムが保証する。

// UT-GPIO-005: 有効チップでライン要求成功（実機のみ）
#[test]
fn request_valid_chip_succeeds() {
    let dev = "/dev/gpiochip0";
    if !std::path::Path::new(dev).exists() {
        eprintln!("skip: {dev} not available");
        return;
    }
    let mut line = GpioLine::new(dev, 26);
    if let Err(e) = line.request_edge_events(Edge::Falling) {
        // チップは存在してもラインが他で使用中の場合がある
        eprintln!("skip: line busy or unavailable: {e}");
        return;
    }
    assert!(line.is_requested());
    assert!(line.event_fd() >= 0);
    // エッジ入力が無い状態でのタイムアウト動作 (Ok(false)) を確認
    assert!(!line.wait_event(10).unwrap());
}
