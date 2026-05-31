#include "adxl345.hpp"
#include "ispi_driver.hpp"
#include "spi_driver.hpp"

#include <memory>

namespace embedded {

namespace reg = adxl345::reg;

struct Adxl345::Impl {
    ISpiDriver* driver;
    bool        owns_driver;  // Impl がドライバを所有しているか

    explicit Impl(const std::string& path)
        : driver(new SpiDriver(path)), owns_driver(true) {}

    explicit Impl(ISpiDriver* drv)
        : driver(drv), owns_driver(false) {}

    ~Impl() { if (owns_driver) delete driver; }

    Impl(const Impl&)            = delete;
    Impl& operator=(const Impl&) = delete;
};

Adxl345::Adxl345(const std::string& spi_path)
    : impl_(std::make_unique<Impl>(spi_path))
{}

Adxl345::Adxl345(ISpiDriver* driver)
    : impl_(std::make_unique<Impl>(driver))
{}

Adxl345::~Adxl345() = default;

bool Adxl345::open() noexcept
{
    ISpiDriver::Config cfg;
    cfg.speed_hz      = 1000000;  // 1 MHz（ADXL345 の SPI 上限 5MHz に対し安全側）
    cfg.bits_per_word = 8;
    cfg.mode          = 3;        // SPI_MODE_3 (CPOL=1, CPHA=1)
    if (!impl_->driver->open(cfg)) {
        return false;
    }

    // DEVID(0x00) が固定値 0xE5 か確認（疎通・誤配線検出）
    auto id = read_device_id();
    if (!id || *id != adxl345::DEVID_VALUE) {
        impl_->driver->close();
        return false;
    }

    // フル分解能（3.9mg/LSB）+ ±16g レンジ
    if (!write_reg(reg::DATA_FORMAT,
                   adxl345::data_format::FULL_RES | adxl345::data_format::RANGE_16G)) {
        impl_->driver->close();
        return false;
    }

    // 測定開始（POWER_CTL の MEASURE ビットを立てる）
    if (!write_reg(reg::POWER_CTL, adxl345::power_ctl::MEASURE)) {
        impl_->driver->close();
        return false;
    }
    return true;
}

void Adxl345::close() noexcept
{
    impl_->driver->close();
}

bool Adxl345::is_open() const noexcept
{
    return impl_->driver->is_open();
}

std::optional<uint8_t> Adxl345::read_reg(uint8_t addr)
{
    // TX: [ READ | addr,  dummy ]   RX: [ _, value ]
    uint8_t tx[2] = {
        static_cast<uint8_t>(adxl345::access::READ | (addr & adxl345::access::ADDR_MASK)),
        0x00
    };
    uint8_t rx[2] = { 0, 0 };
    if (impl_->driver->transfer(tx, rx, 2) < 0) {
        return std::nullopt;
    }
    return rx[1];
}

bool Adxl345::write_reg(uint8_t addr, uint8_t value)
{
    // TX: [ WRITE | addr,  value ]
    uint8_t tx[2] = {
        static_cast<uint8_t>(adxl345::access::WRITE | (addr & adxl345::access::ADDR_MASK)),
        value
    };
    uint8_t rx[2] = { 0, 0 };
    return impl_->driver->transfer(tx, rx, 2) >= 0;
}

// addr/mask/value は同型だが、レジスタ操作の慣用シグネチャ（read-modify-write）として踏襲する
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
bool Adxl345::update_bits(uint8_t addr, uint8_t mask, uint8_t value)
{
    auto current = read_reg(addr);
    if (!current) {
        return false;
    }
    uint8_t updated = static_cast<uint8_t>((*current & ~mask) | (value & mask));
    return write_reg(addr, updated);
}

std::optional<uint8_t> Adxl345::read_device_id()
{
    return read_reg(reg::DEVID);
}

std::optional<Adxl345::AccelRaw> Adxl345::read_raw()
{
    // DATAX0 から 6 バイトを連続(マルチバイト)読み出し
    //   TX: [ READ | MB | DATAX0,  dummy x6 ]
    //   RX: [ _, X0, X1, Y0, Y1, Z0, Z1 ]   ※各軸リトルエンディアン
    uint8_t tx[7] = {
        static_cast<uint8_t>(adxl345::access::READ | adxl345::access::MULTIBYTE |
                             (reg::DATAX0 & adxl345::access::ADDR_MASK)),
        0, 0, 0, 0, 0, 0
    };
    uint8_t rx[7] = { 0, 0, 0, 0, 0, 0, 0 };
    if (impl_->driver->transfer(tx, rx, 7) < 0) {
        return std::nullopt;
    }

    AccelRaw a;
    a.x = static_cast<int16_t>(static_cast<uint16_t>(rx[2] << 8) | rx[1]);
    a.y = static_cast<int16_t>(static_cast<uint16_t>(rx[4] << 8) | rx[3]);
    a.z = static_cast<int16_t>(static_cast<uint16_t>(rx[6] << 8) | rx[5]);
    return a;
}

std::optional<Adxl345::AccelG> Adxl345::read_g()
{
    auto raw = read_raw();
    if (!raw) {
        return std::nullopt;
    }
    return AccelG{
        raw->x * SCALE_G_PER_LSB,
        raw->y * SCALE_G_PER_LSB,
        raw->z * SCALE_G_PER_LSB
    };
}

} // namespace embedded
