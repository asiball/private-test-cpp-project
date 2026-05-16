#include "device.hpp"
#include "ispi_driver.hpp"
#include "spi_driver.hpp"

#include <thread>

namespace embedded {

struct Device::Impl {
    ISpiDriver* driver;
    bool        owns_driver;  // Impl がドライバを所有しているか

    // 実機用（デフォルト）: SpiDriver を内部で生成して所有する
    explicit Impl(const std::string& path)
        : driver(new SpiDriver(path)), owns_driver(true) {}

    // テスト用: 外部から ISpiDriver を差し込む（所有しない）
    explicit Impl(ISpiDriver* drv)
        : driver(drv), owns_driver(false) {}

    ~Impl() { if (owns_driver) delete driver; }
};

Device::Device(const std::string& spi_path)
    : impl_(new Impl(spi_path))
{}

Device::Device(ISpiDriver* driver)
    : impl_(new Impl(driver))
{}

Device::~Device()
{
    delete impl_;
}

bool Device::open()
{
    ISpiDriver::Config cfg;
    cfg.speed_hz      = 1000000;  // 1 MHz
    cfg.bits_per_word = 8;
    cfg.mode          = 0;        // SPI_MODE_0
    return impl_->driver->open(cfg);
}

void Device::close()
{
    impl_->driver->close();
}

bool Device::is_open() const
{
    return impl_->driver->is_open();
}

std::vector<uint8_t> Device::read(uint8_t reg, size_t len)
{
    std::vector<uint8_t> tx(len + 1, 0x00);
    std::vector<uint8_t> rx(len + 1, 0x00);
    tx[0] = reg | 0x80;  // 読み出しビット

    if (impl_->driver->transfer(tx.data(), rx.data(), tx.size()) < 0) {
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
    return impl_->driver->transfer(tx.data(), rx.data(), tx.size()) >= 0;
}

void Device::read_async(uint8_t reg, size_t len, ReadCallback cb)
{
    // @warning Device オブジェクトのライフタイムはコールバック完了まで
    //          呼び出し側が保証すること（detach しているため）。
    //          長期稼働デーモンでは shared_ptr + enable_shared_from_this を検討すること。
    std::thread([this, reg, len, cb]() {
        auto result = this->read(reg, len);
        int  err    = result.empty() ? impl_->driver->last_errno() : 0;
        cb(result, err);
    }).detach();
}

} // namespace embedded
