//! 共通ユーティリティ — C++ の common/include/logger.hpp に相当。
//!
//! Rust では log クレートのファサードをそのまま使う。
//! バックエンドは env_logger (デバッグ用) に差し替え可能。

pub use log::{debug, error, info, warn};

/// アプリケーション起動時にロガーを初期化する。
/// 環境変数 `RUST_LOG` でレベルを制御できる。
pub fn init_logger() {
    let _ = env_logger::builder().try_init();
}
