#include "i2c_driver.hpp"
#include "logger.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <cerrno>
#include <cstring>
#include <cstdint>

namespace embedded {

I2cDriver::I2cDriver(const std::string& device_path)
    : device_path_(device_path), addr_(0), fd_(-1), last_errno_(0)
{}

I2cDriver::~I2cDriver()
{
    close();
}

bool I2cDriver::open(uint16_t addr) noexcept
{
    if (fd_ >= 0) {
        LOGE("I2cDriver::open called while already open: %s", device_path_.c_str());
        return false;
    }
    fd_ = ::open(device_path_.c_str(), O_RDWR);
    if (fd_ < 0) {
        last_errno_ = errno;
        LOGE("I2cDriver::open failed: %s (%s)", device_path_.c_str(), strerror(last_errno_));
        return false;
    }

    // スレーブアドレスを設定（以降の read/write の宛先になる）
    if (ioctl(fd_, I2C_SLAVE, static_cast<unsigned long>(addr)) < 0) {
        last_errno_ = errno;
        LOGE("I2cDriver::open I2C_SLAVE 0x%02x failed: %s", addr, strerror(last_errno_));
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    addr_ = addr;
    LOGI("I2cDriver::open ok: %s addr=0x%02x", device_path_.c_str(), addr);
    return true;
}

void I2cDriver::close() noexcept
{
    if (fd_ >= 0) {
        LOGD("I2cDriver::close %s", device_path_.c_str());
        ::close(fd_);
        fd_ = -1;
    }
}

int I2cDriver::write(const uint8_t* data, size_t len) noexcept
{
    if (fd_ < 0) {
        last_errno_ = EBADF;
        LOGE("I2cDriver::write called on closed bus");
        return -1;
    }
    if (!data) {
        last_errno_ = EINVAL;
        LOGE("I2cDriver::write null pointer");
        return -1;
    }
    if (len == 0) {
        return 0;
    }

    ssize_t n = ::write(fd_, data, len);
    if (n < 0) {
        last_errno_ = errno;
        LOGE("I2cDriver::write failed: len=%zu errno=%s", len, strerror(last_errno_));
        return -1;
    }
    LOGD("I2cDriver::write ok: %zd bytes", n);
    return static_cast<int>(n);
}

int I2cDriver::read(uint8_t* data, size_t len) noexcept
{
    if (fd_ < 0) {
        last_errno_ = EBADF;
        LOGE("I2cDriver::read called on closed bus");
        return -1;
    }
    if (!data) {
        last_errno_ = EINVAL;
        LOGE("I2cDriver::read null pointer");
        return -1;
    }
    if (len == 0) {
        return 0;
    }

    ssize_t n = ::read(fd_, data, len);
    if (n < 0) {
        last_errno_ = errno;
        LOGE("I2cDriver::read failed: len=%zu errno=%s", len, strerror(last_errno_));
        return -1;
    }
    LOGD("I2cDriver::read ok: %zd bytes", n);
    return static_cast<int>(n);
}

int I2cDriver::write_read(const uint8_t* tx, size_t tx_len,
                          uint8_t* rx, size_t rx_len) noexcept
{
    if (fd_ < 0) {
        last_errno_ = EBADF;
        LOGE("I2cDriver::write_read called on closed bus");
        return -1;
    }
    if (!tx || !rx) {
        last_errno_ = EINVAL;
        LOGE("I2cDriver::write_read null pointer: tx=%p rx=%p",
             static_cast<const void*>(tx), static_cast<void*>(rx));
        return -1;
    }
    if (tx_len == 0 || rx_len == 0) {
        last_errno_ = EINVAL;
        LOGE("I2cDriver::write_read zero length: tx_len=%zu rx_len=%zu", tx_len, rx_len);
        return -1;
    }
    if (tx_len > UINT16_MAX || rx_len > UINT16_MAX) {
        last_errno_ = EOVERFLOW;
        LOGE("I2cDriver::write_read length overflow: tx_len=%zu rx_len=%zu", tx_len, rx_len);
        return -1;
    }

    // リピーテッドスタートを伴う結合トランザクション（I2C_RDWR）:
    //   [START] addr+W, tx... [REPEATED START] addr+R, rx... [STOP]
    struct i2c_msg msgs[2];
    msgs[0].addr  = addr_;
    msgs[0].flags = 0;  // 書き込み
    msgs[0].len   = static_cast<uint16_t>(tx_len);
    msgs[0].buf   = const_cast<uint8_t*>(tx);
    msgs[1].addr  = addr_;
    msgs[1].flags = I2C_M_RD;  // 読み出し
    msgs[1].len   = static_cast<uint16_t>(rx_len);
    msgs[1].buf   = rx;

    struct i2c_rdwr_ioctl_data xfer;
    xfer.msgs  = msgs;
    xfer.nmsgs = 2;

    if (ioctl(fd_, I2C_RDWR, &xfer) < 0) {
        last_errno_ = errno;
        LOGE("I2cDriver::write_read I2C_RDWR failed: %s", strerror(last_errno_));
        return -1;
    }
    LOGD("I2cDriver::write_read ok: wrote %zu read %zu", tx_len, rx_len);
    return static_cast<int>(rx_len);
}

} // namespace embedded
