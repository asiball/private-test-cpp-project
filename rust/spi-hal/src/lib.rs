//! SPI ハードウェア抽象化レイヤー
//!
//! C++ 対応:
//!   `ISpiDriver`    → `SpiDriver` トレイト
//!   `SpiDriver`     → `LinuxSpiDriver` 構造体 (/dev/spidevX.Y)
//!   `[[nodiscard]]` → `#[must_use]`
//!   noexcept        → `Result<_, SpiError>` (失敗は値で伝達)

mod driver;

pub use driver::LinuxSpiDriver;

use thiserror::Error;

/// SPI 操作で発生しうるエラー。
///
/// C++ では `last_errno()` で整数を返していたが、
/// Rust では型付きエラーで意図を明示する。
#[derive(Debug, Error)]
pub enum SpiError {
    /// デバイスファイルのオープンまたは ioctl 設定に失敗。
    #[error("デバイスを開けませんでした: {0}")]
    Open(#[source] std::io::Error),

    /// SPI 全二重転送の ioctl に失敗。
    #[error("SPI 転送に失敗しました: {0}")]
    Transfer(#[source] std::io::Error),

    /// `open()` を呼ばずに `transfer()` を呼んだ。
    #[error("デバイスが開かれていません")]
    NotOpen,

    /// open 済みのまま再度 `open()` を呼んだ (C++ 版 SpiDriver::open と同じく拒否する)。
    #[error("デバイスは既に開かれています")]
    AlreadyOpen,

    /// `tx` と `rx` のバッファ長が一致しない。
    #[error("バッファ長が不正です: tx={tx} rx={rx}")]
    LengthMismatch {
        /// 送信バッファ長
        tx: usize,
        /// 受信バッファ長
        rx: usize,
    },

    /// 転送長が `u32` を超過し、ioctl の len フィールドに収まらない。
    #[error("転送長が大きすぎます: {len} バイト (上限 {max})", max = u32::MAX)]
    Overflow {
        /// 要求された転送長
        len: usize,
    },
}

/// SPI バス設定。`ISpiDriver::Config` に相当。
#[derive(Debug, Clone, Copy)]
pub struct SpiConfig {
    /// クロック周波数 \[Hz\]
    pub speed_hz: u32,
    /// ワード幅 (通常 8)
    pub bits_per_word: u8,
    /// SPI モード (0–3)
    pub mode: u8,
}

impl Default for SpiConfig {
    fn default() -> Self {
        Self {
            speed_hz: 1_000_000,
            bits_per_word: 8,
            mode: 0,
        }
    }
}

/// SPI ドライバの抽象インターフェース。
///
/// C++ の `ISpiDriver` 純粋仮想クラスに相当する。実機実装は [`LinuxSpiDriver`]、
/// テストではこのトレイトのモック実装を注入する（依存注入）。
/// `Send` を要求するのは、上位層 (CLI のモニタスレッド等) がドライバを
/// 別スレッドへ移動できるようにするため。
pub trait SpiDriver: Send {
    /// デバイスを設定付きで開く。
    ///
    /// # Errors
    ///
    /// - [`SpiError::AlreadyOpen`] — open 済みのまま再度呼んだ
    /// - [`SpiError::Open`] — デバイスファイルの open または ioctl 設定に失敗
    fn open(&mut self, config: &SpiConfig) -> Result<(), SpiError>;

    /// デバイスを閉じる。未オープン時は何もしない（冪等）。
    ///
    /// `Drop` からも呼ばれるため、明示的に呼ばなくてもリークしない (RAII)。
    fn close(&mut self);

    /// 全二重転送を行う。`tx` の送信と同時に同じ長さの `rx` を受信する。
    ///
    /// # Errors
    ///
    /// - [`SpiError::NotOpen`] — `open()` 前に呼んだ
    /// - [`SpiError::LengthMismatch`] — `tx` と `rx` の長さが異なる
    /// - [`SpiError::Transfer`] — ioctl が失敗した（EAGAIN はリトライ後）
    fn transfer(&mut self, tx: &[u8], rx: &mut [u8]) -> Result<(), SpiError>;

    /// デバイスが現在開かれているか返す。
    #[must_use]
    fn is_open(&self) -> bool;
}
