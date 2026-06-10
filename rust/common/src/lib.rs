//! 共通ユーティリティ — C++ の common/include/logger.hpp に相当。
//!
//! Rust では log クレートのファサードをそのまま使う。
//! バックエンドは env_logger (デバッグ用) に差し替え可能。

pub use log::{debug, error, info, warn};

/// アプリケーション起動時にロガーを初期化する。
///
/// 環境変数 `RUST_LOG` でレベルを制御できる（例: `RUST_LOG=debug`）。
/// 二重初期化は無視されるため、テストから複数回呼んでも安全。
///
/// # Examples
///
/// ```
/// embedded_common::init_logger();
/// embedded_common::info!("起動しました"); // log::info! の再エクスポート
/// embedded_common::init_logger(); // 2 回目は no-op
/// ```
pub fn init_logger() {
    let _ = env_logger::builder().try_init();
}
