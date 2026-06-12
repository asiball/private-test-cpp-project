//! Linux spidev (/dev/spidevX.Y) 実装。
//!
//! C++ の `SpiDriver` クラスに相当。
//! RAII: `Drop` が `close()` を自動呼び出し。
//!
//! Linux カーネルの spidev ioctl を直接呼ぶため `unsafe` が必要。
//! ioctl 呼び出しはすべてカーネルドキュメントに従った正当な操作のみ行う。
#![allow(unsafe_code)]

use std::fs::{File, OpenOptions};
use std::os::unix::io::AsRawFd;

use crate::{SpiConfig, SpiDriver, SpiError};

// ioctl の size フィールド (14bit) 用に構造体サイズを u32 へ変換する。
// ここで扱う型はすべて数十バイトなので切り捨ては起こり得ない。
#[allow(clippy::cast_possible_truncation)]
const fn ioc_size_of<T>() -> u32 {
    std::mem::size_of::<T>() as u32
}

// _IOW(type, nr, size) の Rust 版マクロ (linux/ioctl.h)
// ioctl 番号は 32bit に収まる。libc::ioctl の request 引数は c_ulong で
// armv7(u32) と x86_64(u64) で幅が異なるため、u32 で計算し呼び出し時に拡幅する。
macro_rules! iow {
    ($ty:expr, $nr:expr, $size:ty) => {
        ((1u32 << 30) | (($ty as u32) << 8) | ($nr as u32) | (ioc_size_of::<$size>() << 16))
    };
}

const SPI_IOC_MAGIC: u8 = b'k';
const SPI_IOC_WR_MODE: u32 = iow!(SPI_IOC_MAGIC, 1, u8);
const SPI_IOC_WR_BITS_PER_WORD: u32 = iow!(SPI_IOC_MAGIC, 3, u8);
const SPI_IOC_WR_MAX_SPEED_HZ: u32 = iow!(SPI_IOC_MAGIC, 4, u32);

// SPI_IOC_MESSAGE(1): _IOW('k', 0, spi_ioc_transfer) — nr=0 は SPI_IOC_MESSAGE 専用
// std::mem::size_of は const fn なのでコンパイル時定数として評価される
const fn spi_ioc_message_1() -> u32 {
    (1u32 << 30) | (ioc_size_of::<SpiIocTransfer>() << 16) | ((SPI_IOC_MAGIC as u32) << 8)
}

/// linux/spi/spidev.h の `spi_ioc_transfer` に対応。
#[repr(C)]
struct SpiIocTransfer {
    tx_buf: u64,
    rx_buf: u64,
    len: u32,
    speed_hz: u32,
    delay_usecs: u16,
    bits_per_word: u8,
    cs_change: u8,
    tx_nbits: u8,
    rx_nbits: u8,
    word_delay_usecs: u8,
    _pad: u8,
}

/// `/dev/spidevX.Y` を直接操作する Linux 実装。
///
/// C++ `SpiDriver` クラスに相当。`Drop` 実装により `close()` が
/// 自動呼び出しされる (RAII)。スレッドセーフではないため、
/// 複数スレッドから使う場合は呼び出し側で排他制御すること。
///
/// # Examples
///
/// ```no_run
/// use spi_hal::{LinuxSpiDriver, SpiConfig, SpiDriver};
///
/// fn main() -> Result<(), Box<dyn std::error::Error>> {
///     let mut drv = LinuxSpiDriver::new("/dev/spidev0.0");
///     drv.open(&SpiConfig { speed_hz: 1_000_000, bits_per_word: 8, mode: 0 })?;
///
///     let tx = [0x01, 0x80, 0x00];
///     let mut rx = [0u8; 3];
///     drv.transfer(&tx, &mut rx)?;
///     Ok(())
/// } // drv は Drop で自動クローズされる
/// ```
pub struct LinuxSpiDriver {
    path: String,
    file: Option<File>,
    config: Option<SpiConfig>,
}

impl LinuxSpiDriver {
    /// 指定パスの spidev デバイスに対するドライバを作成する。`open()` するまで fd は開かれない。
    pub fn new(path: impl Into<String>) -> Self {
        Self {
            path: path.into(),
            file: None,
            config: None,
        }
    }
}

impl Drop for LinuxSpiDriver {
    fn drop(&mut self) {
        self.close();
    }
}

impl SpiDriver for LinuxSpiDriver {
    fn open(&mut self, config: &SpiConfig) -> Result<(), SpiError> {
        if self.file.is_some() {
            return Err(SpiError::AlreadyOpen);
        }
        let f = OpenOptions::new()
            .read(true)
            .write(true)
            .open(&self.path)
            .map_err(SpiError::Open)?;

        let fd = f.as_raw_fd();

        // ioctl で SPI パラメータを設定 (linux/spi/spidev.h)
        unsafe {
            let mode = config.mode;
            if libc::ioctl(fd, SPI_IOC_WR_MODE as libc::c_ulong, &mode as *const u8) < 0 {
                return Err(SpiError::Open(std::io::Error::last_os_error()));
            }
            let bpw = config.bits_per_word;
            if libc::ioctl(
                fd,
                SPI_IOC_WR_BITS_PER_WORD as libc::c_ulong,
                &bpw as *const u8,
            ) < 0
            {
                return Err(SpiError::Open(std::io::Error::last_os_error()));
            }
            let speed = config.speed_hz;
            if libc::ioctl(
                fd,
                SPI_IOC_WR_MAX_SPEED_HZ as libc::c_ulong,
                &speed as *const u32,
            ) < 0
            {
                return Err(SpiError::Open(std::io::Error::last_os_error()));
            }
        }

        self.file = Some(f);
        self.config = Some(*config);
        log::debug!("SPI opened: {}", self.path);
        Ok(())
    }

    fn close(&mut self) {
        if self.file.take().is_some() {
            log::debug!("SPI closed: {}", self.path);
        }
    }

    fn transfer(&mut self, tx: &[u8], rx: &mut [u8]) -> Result<(), SpiError> {
        if tx.len() != rx.len() {
            return Err(SpiError::LengthMismatch {
                tx: tx.len(),
                rx: rx.len(),
            });
        }
        let f = self.file.as_ref().ok_or(SpiError::NotOpen)?;
        let cfg = self.config.unwrap_or_default();

        let tr = SpiIocTransfer {
            tx_buf: tx.as_ptr() as u64,
            rx_buf: rx.as_mut_ptr() as u64,
            len: u32::try_from(tx.len()).map_err(|_| SpiError::Overflow {
                len: tx.len(),
            })?,
            speed_hz: cfg.speed_hz,
            delay_usecs: 0,
            bits_per_word: cfg.bits_per_word,
            cs_change: 0,
            tx_nbits: 0,
            rx_nbits: 0,
            word_delay_usecs: 0,
            _pad: 0,
        };

        // EAGAIN は一時的なリソース不足。C++ 版 (SpiDriver::transfer) と同じく最大 3 回試行する
        let mut last_err = std::io::Error::from_raw_os_error(libc::EAGAIN);
        for retry in 0..3 {
            let ret = unsafe {
                libc::ioctl(
                    f.as_raw_fd(),
                    spi_ioc_message_1() as libc::c_ulong,
                    &tr as *const SpiIocTransfer,
                )
            };
            if ret >= 0 {
                return Ok(());
            }
            last_err = std::io::Error::last_os_error();
            if last_err.raw_os_error() != Some(libc::EAGAIN) {
                break;
            }
            log::warn!("SPI transfer EAGAIN retry {}/3", retry + 1);
        }
        Err(SpiError::Transfer(last_err))
    }

    fn is_open(&self) -> bool {
        self.file.is_some()
    }
}
