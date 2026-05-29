#include "kernel_spi_driver.hpp"
#include "my_spi_dev.h"
#include "logger.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cerrno>
#include <cstring>
#include <cstdint>

namespace embedded {

KernelSpiDriver::KernelSpiDriver(const std::string& device_path)
    : device_path_(device_path), fd_(-1), last_errno_(0)
{}

KernelSpiDriver::~KernelSpiDriver()
{
    close();
}

bool KernelSpiDriver::open(const Config& cfg) noexcept
{
    if (fd_ >= 0) {
        LOGE("KernelSpiDriver::open called while already open: %s", device_path_.c_str());
        return false;
    }
    fd_ = ::open(device_path_.c_str(), O_RDWR);
    if (fd_ < 0) {
        last_errno_ = errno;
        LOGE("KernelSpiDriver::open failed: %s (%s)",
             device_path_.c_str(), strerror(last_errno_));
        return false;
    }

    struct my_spi_config kcfg;
    kcfg.speed_hz      = cfg.speed_hz;
    kcfg.bits_per_word = cfg.bits_per_word;
    kcfg.mode          = cfg.mode;

    if (ioctl(fd_, MY_SPI_IOC_CONFIG, &kcfg) < 0) {
        last_errno_ = errno;
        LOGE("KernelSpiDriver::open MY_SPI_IOC_CONFIG failed: %s (%s)",
             device_path_.c_str(), strerror(last_errno_));
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    LOGI("KernelSpiDriver::open ok: %s mode=%u bits=%u speed=%uHz",
         device_path_.c_str(), cfg.mode, cfg.bits_per_word, cfg.speed_hz);
    return true;
}

void KernelSpiDriver::close() noexcept
{
    if (fd_ >= 0) {
        LOGD("KernelSpiDriver::close %s", device_path_.c_str());
        ::close(fd_);
        fd_ = -1;
    }
}

int KernelSpiDriver::transfer(const uint8_t* tx, uint8_t* rx, size_t len) noexcept
{
    if (fd_ < 0) {
        last_errno_ = EBADF;
        LOGE("KernelSpiDriver::transfer called on closed device");
        return -1;
    }
    if (!tx || !rx) {
        last_errno_ = EINVAL;
        LOGE("KernelSpiDriver::transfer null pointer: tx=%p rx=%p",
             static_cast<const void*>(tx), static_cast<void*>(rx));
        return -1;
    }
    if (len == 0) {
        return 0;
    }

    struct my_spi_transfer xfer;
    xfer.tx_buf = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tx));
    xfer.rx_buf = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(rx));
    xfer.len    = static_cast<uint32_t>(len);

    if (ioctl(fd_, MY_SPI_IOC_TRANSFER, &xfer) < 0) {
        last_errno_ = errno;
        LOGE("KernelSpiDriver::transfer MY_SPI_IOC_TRANSFER failed: len=%zu errno=%s",
             len, strerror(last_errno_));
        return -1;
    }

    LOGD("KernelSpiDriver::transfer ok: %zu bytes", len);
    return static_cast<int>(len);
}

} // namespace embedded
