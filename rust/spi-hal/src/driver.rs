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

// _IOW(type, nr, size) の Rust 版マクロ (linux/ioctl.h)
macro_rules! iow {
    ($ty:expr, $nr:expr, $size:ty) => {
        ((1u64 << 30)
            | (($ty as u64) << 8)
            | ($nr as u64)
            | ((std::mem::size_of::<$size>() as u64) << 16))
    };
}

const SPI_IOC_MAGIC: u8 = b'k';
const SPI_IOC_WR_MODE: u64 = iow!(SPI_IOC_MAGIC, 1, u8);
const SPI_IOC_WR_BITS_PER_WORD: u64 = iow!(SPI_IOC_MAGIC, 3, u8);
const SPI_IOC_WR_MAX_SPEED_HZ: u64 = iow!(SPI_IOC_MAGIC, 4, u32);

fn spi_ioc_message_1() -> u64 {
    let size = std::mem::size_of::<SpiIocTransfer>() as u64;
    (1u64 << 30) | (size << 16) | ((SPI_IOC_MAGIC as u64) << 8)
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
/// C++ `SpiDriver` クラスに相当。
/// `Drop` 実装により `close()` が自動呼び出しされる (RAII)。
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
        let f = OpenOptions::new()
            .read(true)
            .write(true)
            .open(&self.path)
            .map_err(SpiError::Open)?;

        let fd = f.as_raw_fd();

        // ioctl で SPI パラメータを設定 (linux/spi/spidev.h)
        unsafe {
            let mode = config.mode;
            if libc::ioctl(fd, SPI_IOC_WR_MODE, &mode as *const u8) < 0 {
                return Err(SpiError::Open(std::io::Error::last_os_error()));
            }
            let bpw = config.bits_per_word;
            if libc::ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bpw as *const u8) < 0 {
                return Err(SpiError::Open(std::io::Error::last_os_error()));
            }
            let speed = config.speed_hz;
            if libc::ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed as *const u32) < 0 {
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
            len: u32::try_from(tx.len()).map_err(|_| SpiError::LengthMismatch {
                tx: tx.len(),
                rx: rx.len(),
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

        let ret = unsafe {
            libc::ioctl(
                f.as_raw_fd(),
                spi_ioc_message_1(),
                &tr as *const SpiIocTransfer,
            )
        };
        if ret < 0 {
            return Err(SpiError::Transfer(std::io::Error::last_os_error()));
        }
        Ok(())
    }

    fn is_open(&self) -> bool {
        self.file.is_some()
    }
}
