//! GPIO エッジ割り込みライブラリ
//!
//! C++ 対応:
//!   `GpioLine`  → `GpioLine` 構造体
//!   epoll + ioctl (GPIO chardev v2 uABI) を Rust で直接実装。
//!
//! GPIO chardev v2 uABI と epoll の ioctl を直接呼ぶため `unsafe` が必要。
//! ADS1115 ALERT/RDY ピンのエッジ検知に使用する。
#![allow(unsafe_code)]

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
const GPIO_V2_GET_LINE_IOCTL: u64 = 0xC250_B407;
const GPIO_V2_LINE_FLAG_INPUT: u64 = 1 << 1;
const GPIO_V2_LINE_FLAG_EDGE_RISING: u64 = 1 << 8;
const GPIO_V2_LINE_FLAG_EDGE_FALLING: u64 = 1 << 9;

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
    last_errno: i32,
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
            last_errno: 0,
        }
    }

    /// エッジ要求。C++ の `request_edge_events()` に相当。
    ///
    /// 再呼び出し時の fd リーク防止のため、まず既存の fd を解放してから再取得する。
    pub fn request_edge_events(&mut self, edge: Edge) -> Result<(), GpioError> {
        self.close();

        use std::ffi::CString;
        let path = CString::new(self.chip_path.as_str()).unwrap();

        let chip_fd = unsafe { libc::open(path.as_ptr(), libc::O_RDONLY | libc::O_CLOEXEC) };
        if chip_fd < 0 {
            self.last_errno = unsafe { *libc::__errno_location() };
            return Err(GpioError::Open(std::io::Error::last_os_error()));
        }
        self.chip_fd = chip_fd;

        let mut flags = GPIO_V2_LINE_FLAG_INPUT;
        match edge {
            Edge::Rising => flags |= GPIO_V2_LINE_FLAG_EDGE_RISING,
            Edge::Falling => flags |= GPIO_V2_LINE_FLAG_EDGE_FALLING,
            Edge::Both => flags |= GPIO_V2_LINE_FLAG_EDGE_RISING | GPIO_V2_LINE_FLAG_EDGE_FALLING,
        }

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
            self.last_errno = unsafe { *libc::__errno_location() };
            return Err(GpioError::Request(std::io::Error::last_os_error()));
        }
        self.line_fd = req.fd;

        let epfd = unsafe { libc::epoll_create1(libc::EPOLL_CLOEXEC) };
        if epfd < 0 {
            return Err(GpioError::Request(std::io::Error::last_os_error()));
        }
        self.epoll_fd = epfd;

        let mut ev = libc::epoll_event {
            events: libc::EPOLLIN as u32,
            u64: 0,
        };
        let ret =
            unsafe { libc::epoll_ctl(self.epoll_fd, libc::EPOLL_CTL_ADD, self.line_fd, &mut ev) };
        if ret < 0 {
            return Err(GpioError::Request(std::io::Error::last_os_error()));
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
    pub fn wait_event(&mut self, timeout_ms: i32) -> Result<bool, GpioError> {
        if self.epoll_fd < 0 {
            return Err(GpioError::NotRequested);
        }
        let mut events = [libc::epoll_event { events: 0, u64: 0 }; 1];
        let ret = unsafe { libc::epoll_wait(self.epoll_fd, events.as_mut_ptr(), 1, timeout_ms) };
        if ret < 0 {
            self.last_errno = unsafe { *libc::__errno_location() };
            return Err(GpioError::Wait(std::io::Error::last_os_error()));
        }
        Ok(ret > 0)
    }

    /// エッジイベントを受け取るファイルディスクリプタを返す。
    pub fn event_fd(&self) -> RawFd {
        self.line_fd
    }

    /// ラインが要求済みかどうかを返す。
    pub fn is_requested(&self) -> bool {
        self.line_fd >= 0
    }

    /// 最後に発生した errno 値を返す。
    pub fn last_errno(&self) -> i32 {
        self.last_errno
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
