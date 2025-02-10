#include "spi_driver.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <cerrno>

namespace embedded {
SpiDriver::SpiDriver(const std::string& device_path)
    : device_path_(device_path), fd_(-1), last_errno_(0) {}
SpiDriver::~SpiDriver() { close(); }
bool SpiDriver::open(const Config& cfg) {
    fd_ = ::open(device_path_.c_str(), O_RDWR);
    if (fd_ < 0) { last_errno_ = errno; return false; }
    if (ioctl(fd_, SPI_IOC_WR_MODE, &cfg.mode) < 0 ||
        ioctl(fd_, SPI_IOC_WR_BITS_PER_WORD, &cfg.bits_per_word) < 0 ||
        ioctl(fd_, SPI_IOC_WR_MAX_SPEED_HZ, &cfg.speed_hz) < 0) {
        last_errno_ = errno; ::close(fd_); fd_ = -1; return false;
    }
    return true;
}
void SpiDriver::close() { if (fd_ >= 0) { ::close(fd_); fd_ = -1; } }
int SpiDriver::transfer(const uint8_t* tx, uint8_t* rx, size_t len) {
    if (fd_ < 0) return -1;
    struct spi_ioc_transfer tr = {};
    tr.tx_buf = reinterpret_cast<uintptr_t>(tx);
    tr.rx_buf = reinterpret_cast<uintptr_t>(rx);
    tr.len = static_cast<uint32_t>(len);
    int ret = ioctl(fd_, SPI_IOC_MESSAGE(1), &tr);
    if (ret < 0) last_errno_ = errno;
    return ret;
}
} // namespace embedded
