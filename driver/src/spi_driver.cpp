#include "spi_driver.hpp"
#include "logger.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <cerrno>
#include <cstring>

namespace embedded {

SpiDriver::SpiDriver(const std::string& device_path)
    : device_path_(device_path), fd_(-1), last_errno_(0)
{}

SpiDriver::~SpiDriver()
{
    close();
}

bool SpiDriver::open(const Config& cfg) noexcept
{
    fd_ = ::open(device_path_.c_str(), O_RDWR);
    if (fd_ < 0) {
        last_errno_ = errno;
        LOGE("SpiDriver::open failed: %s (%s)", device_path_.c_str(), strerror(last_errno_));
        return false;
    }

    if (ioctl(fd_, SPI_IOC_WR_MODE, &cfg.mode) < 0 ||
        ioctl(fd_, SPI_IOC_WR_BITS_PER_WORD, &cfg.bits_per_word) < 0 ||
        ioctl(fd_, SPI_IOC_WR_MAX_SPEED_HZ, &cfg.speed_hz) < 0)
    {
        last_errno_ = errno;
        LOGE("SpiDriver::open ioctl failed: %s (%s)", device_path_.c_str(), strerror(last_errno_));
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    LOGI("SpiDriver::open ok: %s mode=%u bits=%u speed=%uHz",
             device_path_.c_str(), cfg.mode, cfg.bits_per_word, cfg.speed_hz);
    return true;
}

void SpiDriver::close() noexcept
{
    if (fd_ >= 0) {
        LOGD("SpiDriver::close %s", device_path_.c_str());
        ::close(fd_);
        fd_ = -1;
    }
}

int SpiDriver::transfer(const uint8_t* tx, uint8_t* rx, size_t len) noexcept
{
    if (fd_ < 0) {
        LOGE("SpiDriver::transfer called on closed device");
        return -1;
    }
    if (!tx || !rx) {
        LOGE("SpiDriver::transfer null pointer: tx=%p rx=%p",
             static_cast<const void*>(tx), static_cast<void*>(rx));
        return -1;
    }
    if (len == 0) {
        return 0;
    }
    if (len > static_cast<size_t>(UINT32_MAX)) {
        LOGE("SpiDriver::transfer length overflow: %zu", len);
        return -1;
    }

    struct spi_ioc_transfer tr = {};
    tr.tx_buf        = reinterpret_cast<uintptr_t>(tx);
    tr.rx_buf        = reinterpret_cast<uintptr_t>(rx);
    tr.len           = static_cast<uint32_t>(len);
    tr.speed_hz      = 0;
    tr.bits_per_word = 0;

    int ret;
    // EAGAIN は一時的なリソース不足。最大3回リトライする
    for (int retry = 0; retry < 3; ++retry) {
        ret = ioctl(fd_, SPI_IOC_MESSAGE(1), &tr);
        if (ret >= 0 || errno != EAGAIN) break;
        LOGW("SpiDriver::transfer EAGAIN retry %d/3", retry + 1);
    }

    if (ret < 0) {
        last_errno_ = errno;
        LOGE("SpiDriver::transfer failed: len=%zu errno=%s", len, strerror(last_errno_));
    } else {
        LOGD("SpiDriver::transfer ok: %d bytes", ret);
    }
    return ret;
}

} // namespace embedded
