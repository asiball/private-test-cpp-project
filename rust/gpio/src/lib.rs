//! GPIO エッジ割り込みライブラリ
//!
//! C++ 対応:
//!   `GpioLine`  → `GpioLine` 構造体
//!   epoll + ioctl (GPIO chardev v2 uABI) を Rust で直接実装。
//!
//! GPIO chardev v2 uABI と epoll の ioctl を直接呼ぶため `unsafe` が必要。
//! ADS1115 ALERT/RDY ピンのエッジ検知に使用する。
#![allow(unsafe_code)]

use std::ffi::CString;
use std::os::unix::io::RawFd;
use thiserror::Error;

/// GPIO 操作で発生しうるエラー。
#[derive(Debug, Error)]
pub enum GpioError {
    /// GPIO チップデバイスファイルのオープンに失敗。
    #[error("GPIO チップを開けませんでした: {0}")]
    Open(#[source] std::io::Error),

    /// GPIO v2 ライン要求 ioctl に失敗。
    #[error("ライン要求に失敗しました: {0}")]
    Request(#[source] std::io::Error),

    /// epoll_wait でエラーが発生した。
    #[error("epoll 待機エラー: {0}")]
    Wait(#[source] std::io::Error),

    /// GPIO イベントの読み出しに失敗。
    #[error("イベント読み出しエラー: {0}")]
    Read(#[source] std::io::Error),

    /// `request_edge_events()` を呼ばずにイベント待機を試みた。
    #[error("デバイスが要求されていません")]
    NotRequested,
}

/// エッジ検知の種類。C++ `GpioLine::Edge` enum に相当。
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Edge {
    /// 立ち上がりエッジのみ検知。
    Rising,
    /// 立ち下がりエッジのみ検知。
    Falling,
    /// 立ち上がり・立ち下がり両エッジを検知。
    Both,
}

// Linux GPIO chardev v2 uABI 定数 (linux/gpio.h)
// GPIO_V2_GET_LINE_IOCTL: _IOWR(0xB4, 7, gpio_v2_line_request) where sizeof = 592
const GPIO_V2_GET_LINE_IOCTL: u32 = 0xC250_B407;
// enum gpio_v2_line_flag (linux/gpio.h):
//   bit0=USED bit1=ACTIVE_LOW bit2=INPUT bit3=OUTPUT
//   bit4=EDGE_RISING bit5=EDGE_FALLING bit6=OPEN_DRAIN ...
const GPIO_V2_LINE_FLAG_INPUT: u64 = 1 << 2;
const GPIO_V2_LINE_FLAG_EDGE_RISING: u64 = 1 << 4;
const GPIO_V2_LINE_FLAG_EDGE_FALLING: u64 = 1 << 5;

// gpio_v2_line_config (linux/gpio.h) — 合計 272 bytes
#[repr(C)]
struct GpioV2LineConfig {
    flags: u64,
    num_attrs: u32,
    _padding: [u32; 5],
    attrs: [u8; 240], // gpio_v2_line_config_attribute[10] の正しいサイズ
}

// gpio_v2_line_request (linux/gpio.h) — 合計 592 bytes
#[repr(C)]
struct GpioV2LineRequest {
    offsets: [u32; 64],
    consumer: [u8; 32],
    config: GpioV2LineConfig,
    num_lines: u32,
    event_buffer_size: u32,
    _padding: [u32; 5],
    fd: i32,
}

/// linux/gpio.h `struct gpio_v2_line_event` — 合計 48 bytes。
/// カーネルはこのサイズ未満の read() を EINVAL で拒否するため、
/// 必ず構造体全体を読み出す必要がある。
#[repr(C)]
#[derive(Clone, Copy)]
struct GpioV2LineEvent {
    timestamp_ns: u64,
    id: u32,
    offset: u32,
    seqno: u32,
    line_seqno: u32,
    _padding: [u32; 6],
}

/// GPIO 単一ラインのエッジ検知。
///
/// C++ `GpioLine` に相当。
/// Drop で fd を自動クローズ (RAII)。
pub struct GpioLine {
    chip_path: String,
    offset: u32,
    chip_fd: RawFd,
    line_fd: RawFd,
    epoll_fd: RawFd,
}

impl GpioLine {
    /// GPIO チップパス (`/dev/gpiochipN`) とライン番号を指定してインスタンスを作成する。
    pub fn new(chip_path: impl Into<String>, offset: u32) -> Self {
        Self {
            chip_path: chip_path.into(),
            offset,
            chip_fd: -1,
            line_fd: -1,
            epoll_fd: -1,
        }
    }

    /// エッジ要求。C++ の `request_edge_events()` に相当。
    ///
    /// 再呼び出し時の fd リーク防止のため、まず既存の fd を解放してから再取得する。
    pub fn request_edge_events(&mut self, edge: Edge) -> Result<(), GpioError> {
        self.close();

        // パスにヌルバイトが含まれていたらパニックせず Err を返す
        let path = CString::new(self.chip_path.as_str()).map_err(|e| {
            GpioError::Open(std::io::Error::new(std::io::ErrorKind::InvalidInput, e))
        })?;

        let chip_fd = unsafe { libc::open(path.as_ptr(), libc::O_RDONLY | libc::O_CLOEXEC) };
        if chip_fd < 0 {
            return Err(GpioError::Open(std::io::Error::last_os_error()));
        }
        self.chip_fd = chip_fd;

        let mut flags = GPIO_V2_LINE_FLAG_INPUT;
        match edge {
            Edge::Rising => flags |= GPIO_V2_LINE_FLAG_EDGE_RISING,
            Edge::Falling => flags |= GPIO_V2_LINE_FLAG_EDGE_FALLING,
            Edge::Both => flags |= GPIO_V2_LINE_FLAG_EDGE_RISING | GPIO_V2_LINE_FLAG_EDGE_FALLING,
        }

        let mut req: GpioV2LineRequest = unsafe { std::mem::zeroed() };
        req.offsets[0] = self.offset;
        req.num_lines = 1;
        req.config.flags = flags;

        let consumer = b"embedded-gpio\0";
        req.consumer[..consumer.len()].copy_from_slice(consumer);

        let ret = unsafe {
            libc::ioctl(
                self.chip_fd,
                GPIO_V2_GET_LINE_IOCTL as libc::c_ulong,
                &mut req,
            )
        };
        if ret < 0 {
            let err = std::io::Error::last_os_error();
            self.close();
            return Err(GpioError::Request(err));
        }
        self.line_fd = req.fd;

        let epfd = unsafe { libc::epoll_create1(libc::EPOLL_CLOEXEC) };
        if epfd < 0 {
            let err = std::io::Error::last_os_error();
            self.close();
            return Err(GpioError::Request(err));
        }
        self.epoll_fd = epfd;

        let mut ev = libc::epoll_event {
            events: libc::EPOLLIN as u32,
            u64: 0,
        };
        let ret =
            unsafe { libc::epoll_ctl(self.epoll_fd, libc::EPOLL_CTL_ADD, self.line_fd, &mut ev) };
        if ret < 0 {
            let err = std::io::Error::last_os_error();
            self.close();
            return Err(GpioError::Request(err));
        }

        log::debug!(
            "GPIO line requested: {} offset={}",
            self.chip_path,
            self.offset
        );
        Ok(())
    }

    /// エッジイベント待機。C++ の `wait_event()` に相当。
    ///
    /// 戻り値: `Ok(true)` = イベント発生, `Ok(false)` = タイムアウト
    ///
    /// イベントが発生した場合は `gpio_v2_line_event` をドレインする。
    /// ドレインしないと次回以降の epoll_wait が即時返却し続ける (level-triggered)。
    pub fn wait_event(&mut self, timeout_ms: i32) -> Result<bool, GpioError> {
        if self.epoll_fd < 0 {
            return Err(GpioError::NotRequested);
        }
        let mut events = [libc::epoll_event { events: 0, u64: 0 }; 1];
        let ret = unsafe { libc::epoll_wait(self.epoll_fd, events.as_mut_ptr(), 1, timeout_ms) };
        if ret < 0 {
            return Err(GpioError::Wait(std::io::Error::last_os_error()));
        }
        if ret == 0 {
            return Ok(false); // タイムアウト
        }

        // イベントデータを読み出してカーネルバッファをドレインする。
        // 読まないと epoll が EPOLLIN を解除せず、次回以降の呼び出しが即時返却し続ける。
        let mut event: GpioV2LineEvent = unsafe { std::mem::zeroed() };
        let n = unsafe {
            libc::read(
                self.line_fd,
                std::ptr::from_mut(&mut event).cast(),
                std::mem::size_of::<GpioV2LineEvent>(),
            )
        };
        if n < 0 {
            return Err(GpioError::Read(std::io::Error::last_os_error()));
        }
        if n != std::mem::size_of::<GpioV2LineEvent>() as isize {
            return Err(GpioError::Read(std::io::Error::new(
                std::io::ErrorKind::UnexpectedEof,
                format!("短い read: {n} bytes"),
            )));
        }
        log::debug!(
            "GPIO event: id={} offset={} seqno={}",
            event.id,
            event.offset,
            event.seqno
        );

        Ok(true)
    }

    /// エッジイベントを受け取るファイルディスクリプタを返す。
    #[must_use]
    pub fn event_fd(&self) -> RawFd {
        self.line_fd
    }

    /// ラインが要求済みかどうかを返す。
    #[must_use]
    pub fn is_requested(&self) -> bool {
        self.line_fd >= 0
    }

    /// 取得したすべての fd を閉じる。
    pub fn close(&mut self) {
        if self.line_fd >= 0 {
            // close の戻り値 (EINTR など) は再試行しないのが POSIX の推奨
            let _ = unsafe { libc::close(self.line_fd) };
            self.line_fd = -1;
        }
        if self.epoll_fd >= 0 {
            let _ = unsafe { libc::close(self.epoll_fd) };
            self.epoll_fd = -1;
        }
        if self.chip_fd >= 0 {
            let _ = unsafe { libc::close(self.chip_fd) };
            self.chip_fd = -1;
        }
    }
}

impl Drop for GpioLine {
    fn drop(&mut self) {
        self.close();
    }
}

// uABI 定数・構造体レイアウトの回帰テスト。
// linux/gpio.h を手書きで写しているため、カーネル定義との一致をテストで固定する
// （過去にフラグのビット位置とイベントサイズの誤りで実機動作しなかった実績あり）。
#[cfg(test)]
mod uabi_tests {
    use super::*;

    // enum gpio_v2_line_flag (linux/gpio.h)
    #[test]
    fn line_flags_match_kernel_uabi() {
        assert_eq!(GPIO_V2_LINE_FLAG_INPUT, 1 << 2, "INPUT は bit2");
        assert_eq!(GPIO_V2_LINE_FLAG_EDGE_RISING, 1 << 4, "EDGE_RISING は bit4");
        assert_eq!(
            GPIO_V2_LINE_FLAG_EDGE_FALLING,
            1 << 5,
            "EDGE_FALLING は bit5"
        );
    }

    // _IOWR(0xB4, 0x07, struct gpio_v2_line_request)
    #[test]
    fn get_line_ioctl_number_matches_kernel_uabi() {
        assert_eq!(GPIO_V2_GET_LINE_IOCTL, 0xC250_B407);
    }

    // struct gpio_v2_line_event: timestamp_ns(8) + id/offset/seqno/line_seqno(16) + padding(24)
    #[test]
    fn line_event_size_is_48_bytes() {
        assert_eq!(std::mem::size_of::<GpioV2LineEvent>(), 48);
    }

    // ioctl 番号にエンコードされるサイズ (592) と構造体定義の一致を固定する
    #[test]
    fn request_struct_layout_matches_kernel_uabi() {
        assert_eq!(std::mem::size_of::<GpioV2LineConfig>(), 272);
        assert_eq!(std::mem::size_of::<GpioV2LineRequest>(), 592);
    }
}
