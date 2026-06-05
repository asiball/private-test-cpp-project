//! I2C ハードウェア抽象化レイヤー
//!
//! C++ 対応:
//!   `II2cDriver`  → `I2cDriver` トレイト
//!   `I2cDriver`   → `LinuxI2cDriver` 構造体 (/dev/i2c-N)

use std::fs::{File, OpenOptions};
use std::os::unix::io::AsRawFd;
use thiserror::Error;

/// I2C 操作で発生しうるエラー。
#[derive(Debug, Error)]
pub enum I2cError {
    #[error("デバイスを開けませんでした: {0}")]
    Open(#[source] std::io::Error),

    #[error("I2C 書き込みに失敗しました: {0}")]
    Write(#[source] std::io::Error),

    #[error("I2C 読み出しに失敗しました: {0}")]
    Read(#[source] std::io::Error),

    #[error("デバイスが開かれていません")]
    NotOpen,
}

/// I2C ドライバの抽象インターフェース。`II2cDriver` に相当。
pub trait I2cDriver: Send {
    fn open(&mut self, addr: u16) -> Result<(), I2cError>;
    fn close(&mut self);
    fn write(&mut self, data: &[u8]) -> Result<usize, I2cError>;
    fn read(&mut self, buf: &mut [u8]) -> Result<usize, I2cError>;
    /// I2C リピーテッドスタート (レジスタポインタ書き込み → 読み出し)。
    fn write_read(&mut self, tx: &[u8], rx: &mut [u8]) -> Result<(), I2cError>;
    fn is_open(&self) -> bool;
}

// linux/i2c-dev.h
const I2C_SLAVE: u64 = 0x0703;

/// `/dev/i2c-N` を直接操作する Linux 実装。
pub struct LinuxI2cDriver {
    bus_path: String,
    file: Option<File>,
}

impl LinuxI2cDriver {
    pub fn new(bus_path: impl Into<String>) -> Self {
        Self { bus_path: bus_path.into(), file: None }
    }
}

impl Drop for LinuxI2cDriver {
    fn drop(&mut self) {
        self.close();
    }
}

impl I2cDriver for LinuxI2cDriver {
    fn open(&mut self, addr: u16) -> Result<(), I2cError> {
        let f = OpenOptions::new()
            .read(true)
            .write(true)
            .open(&self.bus_path)
            .map_err(I2cError::Open)?;

        // スレーブアドレスを設定
        let ret = unsafe { libc::ioctl(f.as_raw_fd(), I2C_SLAVE, addr as libc::c_ulong) };
        if ret < 0 {
            return Err(I2cError::Open(std::io::Error::last_os_error()));
        }

        self.file = Some(f);
        log::debug!("I2C opened: {} addr=0x{:02X}", self.bus_path, addr);
        Ok(())
    }

    fn close(&mut self) {
        if self.file.take().is_some() {
            log::debug!("I2C closed: {}", self.bus_path);
        }
    }

    fn write(&mut self, data: &[u8]) -> Result<usize, I2cError> {
        use std::io::Write as _;
        let f = self.file.as_mut().ok_or(I2cError::NotOpen)?;
        f.write(data).map_err(I2cError::Write)
    }

    fn read(&mut self, buf: &mut [u8]) -> Result<usize, I2cError> {
        use std::io::Read as _;
        let f = self.file.as_mut().ok_or(I2cError::NotOpen)?;
        f.read(buf).map_err(I2cError::Read)
    }

    fn write_read(&mut self, tx: &[u8], rx: &mut [u8]) -> Result<(), I2cError> {
        self.write(tx)?;
        let n = self.read(rx)?;
        if n != rx.len() {
            return Err(I2cError::Read(std::io::Error::new(
                std::io::ErrorKind::UnexpectedEof,
                format!("期待 {} バイト, 受信 {} バイト", rx.len(), n),
            )));
        }
        Ok(())
    }

    fn is_open(&self) -> bool {
        self.file.is_some()
    }
}
