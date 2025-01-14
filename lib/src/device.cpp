#include "device.hpp"
#include "../../driver/include/spi_driver.hpp"

#include <thread>
#include <stdexcept>

namespace embedded {

struct Device::Impl {
    SpiDriver driver;

    explicit Impl(const std::string& path) : driver(path) {}
};

Device::Device(const std::string& spi_path)
    : impl_(new Impl(spi_path))
{}

Device::~Device()
{
    delete impl_;
}

bool Device::open()
{
    SpiDriver::Config cfg;
    cfg.speed_hz      = 1000000;  // 1 MHz
    cfg.bits_per_word = 8;
    cfg.mode          = 0;        // SPI_MODE_0
    return impl_->driver.open(cfg);
}

void Device::close()
{
    impl_->driver.close();
}

bool Device::is_open() const
{
    return impl_->driver.is_open();
}

std::vector<uint8_t> Device::read(uint8_t reg, size_t len)
{
    std::vector<uint8_t> tx(len + 1, 0x00);
    std::vector<uint8_t> rx(len + 1, 0x00);
    tx[0] = reg | 0x80;  // 読み出しビット

    if (impl_->driver.transfer(tx.data(), rx.data(),
                                static_cast<uint32_t>(tx.size())) < 0) {
        return {};
    }
    return std::vector<uint8_t>(rx.begin() + 1, rx.end());
}

bool Device::write(uint8_t reg, const std::vector<uint8_t>& data)
{
    std::vector<uint8_t> tx;
    tx.reserve(data.size() + 1);
    tx.push_back(reg & 0x7F);
    tx.insert(tx.end(), data.begin(), data.end());

    std::vector<uint8_t> rx(tx.size(), 0);
    return impl_->driver.transfer(tx.data(), rx.data(),
                                   static_cast<uint32_t>(tx.size())) >= 0;
}

void Device::read_async(uint8_t reg, size_t len, ReadCallback cb)
{
    // スレッドをデタッチして非同期実行する。ライフタイムはcbが管理する
    std::thread([this, reg, len, cb]() {
        auto result = this->read(reg, len);
        cb(result, result.empty() ? impl_->driver.last_errno() : 0);
    }).detach();
}

} // namespace embedded
