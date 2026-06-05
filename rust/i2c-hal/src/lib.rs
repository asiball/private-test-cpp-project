//! I2C ハードウェア抽象化レイヤー
//!
//! C++ 対応:
//!   `II2cDriver`  → `I2cDriver` トレイト
//!   `I2cDriver`   → `LinuxI2cDriver` 構造体 (/dev/i2c-N)
//!
//! Linux I2C_SLAVE / I2C_RDWR ioctl を直接呼ぶため `unsafe` が必要。
#![allow(unsafe_code)]

use std::fs::{File, OpenOptions};
use std::os::unix::io::AsRawFd;
use thiserror::Error;

/// I2C 操作で発生しうるエラー。
#[derive(Debug, Error)]
pub enum I2cError {
    /// デバイスファイルのオープンまたはスレーブアドレス設定に失敗。
    #[error("デバイスを開けませんでした: {0}")]
    Open(#[source] std::io::Error),

    /// I2C バスへの書き込みに失敗。
    #[error("I2C 書き込みに失敗しました: {0}")]
    Write(#[source] std::io::Error),

    /// I2C バスからの読み出しに失敗。
    #[error("I2C 読み出しに失敗しました: {0}")]
    Read(#[source] std::io::Error),

    /// `open()` を呼ばずに操作を試みた。
    #[error("デバイスが開かれていません")]
    NotOpen,

    /// バッファ長が I2C メッセージの上限 (65535 バイト) を超えた。
    #[error("バッファ長が上限を超えました: {0} バイト")]
    BufferTooLarge(usize),
}

/// I2C ドライバの抽象インターフェース。`II2cDriver` に相当。
pub trait I2cDriver: Send {
    /// 指定スレーブアドレスでデバイスを開く。
    fn open(&mut self, addr: u16) -> Result<(), I2cError>;
    /// デバイスを閉じる。`Drop` で自動的に呼ばれる設計を推奨。
    fn close(&mut self);
    /// 全バイトを書き込む。部分書き込みはエラーとして扱う。
    fn write(&mut self, data: &[u8]) -> Result<(), I2cError>;
    /// バイト列を読み出す。
    fn read(&mut self, buf: &mut [u8]) -> Result<usize, I2cError>;
    /// I2C Repeated Start: 書き込み→読み出しをアトミックに実行 (I2C_RDWR ioctl)。
    fn write_read(&mut self, tx: &[u8], rx: &mut [u8]) -> Result<(), I2cError>;
    /// デバイスが現在開かれているか返す。
    fn is_open(&self) -> bool;
}

// linux/i2c-dev.h
const I2C_SLAVE: u64 = 0x0703;
const I2C_RDWR: u64 = 0x0707;
const I2C_M_RD: u16 = 0x0001;

/// linux/i2c.h `i2c_msg`
#[repr(C)]
struct I2cMsg {
    addr: u16,
    flags: u16,
    len: u16,
    buf: *mut u8,
}

/// linux/i2c-dev.h `i2c_rdwr_ioctl_data`
#[repr(C)]
struct I2cRdwrIoctlData {
    msgs: *mut I2cMsg,
    nmsgs: u32,
}

/// `/dev/i2c-N` を直接操作する Linux 実装。
pub struct LinuxI2cDriver {
    bus_path: String,
    file: Option<File>,
    addr: u16,
}

impl LinuxI2cDriver {
    /// 指定バスパス (`/dev/i2c-N`) に対するドライバを作成する。
    pub fn new(bus_path: impl Into<String>) -> Self {
        Self {
            bus_path: bus_path.into(),
            file: None,
            addr: 0,
        }
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

        let ret = unsafe { libc::ioctl(f.as_raw_fd(), I2C_SLAVE, addr as libc::c_ulong) };
        if ret < 0 {
            return Err(I2cError::Open(std::io::Error::last_os_error()));
        }

        self.file = Some(f);
        self.addr = addr;
        log::debug!("I2C opened: {} addr=0x{:02X}", self.bus_path, addr);
        Ok(())
    }

    fn close(&mut self) {
        if self.file.take().is_some() {
            log::debug!("I2C closed: {}", self.bus_path);
        }
    }

    fn write(&mut self, data: &[u8]) -> Result<(), I2cError> {
        use std::io::Write as _;
        let f = self.file.as_mut().ok_or(I2cError::NotOpen)?;
        f.write_all(data).map_err(I2cError::Write)
    }

    fn read(&mut self, buf: &mut [u8]) -> Result<usize, I2cError> {
        use std::io::Read as _;
        let f = self.file.as_mut().ok_or(I2cError::NotOpen)?;
        f.read(buf).map_err(I2cError::Read)
    }

    fn write_read(&mut self, tx: &[u8], rx: &mut [u8]) -> Result<(), I2cError> {
        let f = self.file.as_ref().ok_or(I2cError::NotOpen)?;

        let tx_len = u16::try_from(tx.len()).map_err(|_| I2cError::BufferTooLarge(tx.len()))?;
        let rx_len = u16::try_from(rx.len()).map_err(|_| I2cError::BufferTooLarge(rx.len()))?;

        let mut tx_buf = tx.to_vec();

        let mut msgs = [
            I2cMsg {
                addr: self.addr,
                flags: 0,
                len: tx_len,
                buf: tx_buf.as_mut_ptr(),
            },
            I2cMsg {
                addr: self.addr,
                flags: I2C_M_RD,
                len: rx_len,
                buf: rx.as_mut_ptr(),
            },
        ];

        let mut data = I2cRdwrIoctlData {
            msgs: msgs.as_mut_ptr(),
            nmsgs: 2,
        };

        let ret = unsafe { libc::ioctl(f.as_raw_fd(), I2C_RDWR as libc::c_ulong, &mut data) };
        if ret < 0 {
            return Err(I2cError::Read(std::io::Error::last_os_error()));
        }
        Ok(())
    }

    fn is_open(&self) -> bool {
        self.file.is_some()
    }
}
