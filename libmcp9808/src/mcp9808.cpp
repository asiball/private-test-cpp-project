#include "mcp9808.hpp"
#include "ii2c_driver.hpp"
#include "i2c_driver.hpp"

#include <memory>

namespace embedded {

namespace {
// MCP9808 レジスタポインタ
constexpr uint8_t REG_T_AMBIENT = 0x05;  // アンビエント温度（16bit, 上位 3bit はフラグ）
constexpr uint8_t REG_MANUF_ID  = 0x06;  // 製造者 ID（0x0054）

// アンビエント温度レジスタのビット構成
constexpr uint16_t T_FLAG_MASK = 0x1F00;  // 上位バイトの温度ビット（フラグ 15:13 を除く）
constexpr uint8_t  T_SIGN_BIT  = 0x10;    // 上位バイト bit4 = 符号（1 で負）
constexpr double   T_LSB_C     = 0.0625;  // 下位バイトの 1 LSB = 0.0625 °C
} // namespace

struct Mcp9808::Impl {
    std::unique_ptr<II2cDriver> owned;   // 自前生成時のみ所有（注入時は空）
    II2cDriver* driver;                  // 実際に使う非所有ビュー
    uint16_t    addr;

    Impl(const std::string& path, uint16_t a)
        : owned(std::make_unique<I2cDriver>(path)), driver(owned.get()), addr(a) {}

    Impl(II2cDriver* drv, uint16_t a)
        : driver(drv), addr(a) {}

    Impl(const Impl&)            = delete;
    Impl& operator=(const Impl&) = delete;

    // 16bit レジスタを読み出す（リピーテッドスタート, MSB first）
    bool read_reg16(uint8_t reg, uint16_t& out) {
        uint8_t rx[2] = {};
        if (driver->write_read(&reg, 1, rx, 2) < 0) return false;
        out = static_cast<uint16_t>((rx[0] << 8) | rx[1]);
        return true;
    }
};

Mcp9808::Mcp9808(const std::string& i2c_path, uint16_t addr)
    : impl_(std::make_unique<Impl>(i2c_path, addr))
{}

Mcp9808::Mcp9808(II2cDriver* driver, uint16_t addr)
    : impl_(std::make_unique<Impl>(driver, addr))
{}

Mcp9808::~Mcp9808() = default;

bool Mcp9808::open() noexcept
{
    if (!impl_->driver->open(impl_->addr)) {
        return false;
    }
    // 製造者 ID(0x0054) を確認して疎通・誤配線を検出する
    uint16_t id = 0;
    if (!impl_->read_reg16(REG_MANUF_ID, id) || id != MANUFACTURER_ID) {
        impl_->driver->close();
        return false;
    }
    return true;
}

void Mcp9808::close() noexcept
{
    impl_->driver->close();
}

bool Mcp9808::is_open() const noexcept
{
    return impl_->driver->is_open();
}

std::optional<uint16_t> Mcp9808::read_manufacturer_id() noexcept
{
    uint16_t id = 0;
    if (!impl_->read_reg16(REG_MANUF_ID, id)) {
        return std::nullopt;
    }
    return id;
}

std::optional<uint16_t> Mcp9808::read_temp_raw() noexcept
{
    uint16_t raw = 0;
    if (!impl_->read_reg16(REG_T_AMBIENT, raw)) {
        return std::nullopt;
    }
    return raw;
}

std::optional<double> Mcp9808::read_temperature() noexcept
{
    auto raw = read_temp_raw();
    if (!raw) {
        return std::nullopt;
    }
    // 上位バイト: bit4=符号, bit3:0 が整数部の上位。フラグ(15:13)はマスクで除去。
    uint8_t upper = static_cast<uint8_t>((*raw & T_FLAG_MASK) >> 8);
    uint8_t lower = static_cast<uint8_t>(*raw & 0xFF);
    double  temp  = (upper & 0x0F) * 16.0 + lower * T_LSB_C;
    if (upper & T_SIGN_BIT) {
        temp -= 256.0;  // 負の温度（2 の補数相当）
    }
    return temp;
}

} // namespace embedded
