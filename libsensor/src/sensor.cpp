#include "sensor.hpp"
#include "ispi_driver.hpp"
#include "spi_driver.hpp"

#include <memory>
#include <thread>

namespace embedded {

struct Sensor::Impl {
    ISpiDriver* driver;
    bool        owns_driver;  // Impl がドライバを所有しているか
    double      vref_volts;

    explicit Impl(const std::string& path, double v)
        : driver(new SpiDriver(path)), owns_driver(true), vref_volts(v) {}

    explicit Impl(ISpiDriver* drv, double v)
        : driver(drv), owns_driver(false), vref_volts(v) {}

    ~Impl() { if (owns_driver) delete driver; }

    Impl(const Impl&)            = delete;
    Impl& operator=(const Impl&) = delete;
};

Sensor::Sensor(const std::string& spi_path, double vref)
    : impl_(std::make_unique<Impl>(spi_path, vref))
{}

Sensor::Sensor(ISpiDriver* driver, double vref)
    : impl_(std::make_unique<Impl>(driver, vref))
{}

Sensor::~Sensor() = default;

bool Sensor::open() noexcept
{
    ISpiDriver::Config cfg;
    cfg.speed_hz      = 1000000;  // 1 MHz（MCP3008 の Vdd=3.3V 時の上限は 1.35 MHz）
    cfg.bits_per_word = 8;
    cfg.mode          = 0;        // SPI_MODE_0
    return impl_->driver->open(cfg);
}

void Sensor::close() noexcept
{
    impl_->driver->close();
}

bool Sensor::is_open() const noexcept
{
    return impl_->driver->is_open();
}

std::optional<uint16_t> Sensor::read_raw(uint8_t channel)
{
    if (channel >= CHANNEL_COUNT) {
        return std::nullopt;
    }

    // MCP3008 シングルエンドモード:
    //   TX: [ 0x01,  0x80 | (channel << 4),  0x00 ]
    //   RX: [   _,        ----- 10 bit -----      ]
    uint8_t tx[3] = { 0x01,
                      static_cast<uint8_t>(0x80 | (channel << 4)),
                      0x00 };
    uint8_t rx[3] = { 0, 0, 0 };

    if (impl_->driver->transfer(tx, rx, 3) < 0) {
        return std::nullopt;
    }
    uint16_t raw = static_cast<uint16_t>((rx[1] & 0x03) << 8) | rx[2];
    return raw;
}

std::optional<double> Sensor::read_voltage(uint8_t channel)
{
    auto raw = read_raw(channel);
    if (!raw) return std::nullopt;
    return static_cast<double>(*raw) * impl_->vref_volts / ADC_MAX;
}

void Sensor::read_raw_async(uint8_t channel, ReadCallback cb)
{
    // Sensor オブジェクトのライフタイムはコールバック完了まで呼び出し側が保証すること
    // （detach しているため）。長期稼働デーモンでは shared_ptr + enable_shared_from_this を検討。
    std::thread([this, channel, cb]() {
        auto result = this->read_raw(channel);
        int  err    = result ? 0 : impl_->driver->last_errno();
        cb(result, err);
    }).detach();
}

double Sensor::vref() const noexcept
{
    return impl_->vref_volts;
}

void Sensor::set_vref(double v) noexcept
{
    impl_->vref_volts = v;
}

} // namespace embedded
