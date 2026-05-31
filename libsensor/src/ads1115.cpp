#include "ads1115.hpp"
#include "ii2c_driver.hpp"
#include "i2c_driver.hpp"

#include <memory>
#include <unistd.h>

namespace embedded {

namespace {
// ADS1115 レジスタポインタ
constexpr uint8_t REG_CONVERSION = 0x00;
constexpr uint8_t REG_CONFIG     = 0x01;
constexpr uint8_t REG_LO_THRESH  = 0x02;
constexpr uint8_t REG_HI_THRESH  = 0x03;

// Config レジスタ（16bit）のフィールド
constexpr uint16_t CFG_OS_SINGLE   = 0x8000;  // bit15: 1 書き込みでシングルショット開始 / 読み出しで 1=変換完了
constexpr uint16_t CFG_MUX_SINGLE  = 0x4000;  // bits14:12 の単電源ベース（0b100 << 12）。channel を OR する
constexpr uint16_t CFG_MODE_SINGLE = 0x0100;  // bit8: 1=シングルショット
constexpr uint16_t CFG_DR_128SPS   = 0x0080;  // bits7:5: 0b100=128SPS
constexpr uint16_t CFG_COMP_QUE_DISABLE = 0x0003;  // bits1:0: コンパレータ無効
constexpr uint16_t CFG_COMP_QUE_ONE     = 0x0000;  // bits1:0: 1 変換後にアサート（RDY ピン用）

// PGA（ゲイン）→ Config bits11:9
constexpr uint16_t pga_bits(Ads1115::Gain g) {
    return static_cast<uint16_t>(static_cast<uint8_t>(g) & 0x07) << 9;
}
// PGA → フルスケール電圧 [V]
constexpr double full_scale(Ads1115::Gain g) {
    switch (g) {
        case Ads1115::Gain::FSR_6_144V: return 6.144;
        case Ads1115::Gain::FSR_4_096V: return 4.096;
        case Ads1115::Gain::FSR_2_048V: return 2.048;
        case Ads1115::Gain::FSR_1_024V: return 1.024;
        case Ads1115::Gain::FSR_0_512V: return 0.512;
        case Ads1115::Gain::FSR_0_256V: return 0.256;
    }
    return 2.048;
}

constexpr int    MAX_CONVERSION_POLLS = 16;   // OS ビットのポーリング上限
constexpr useconds_t POLL_INTERVAL_US = 500;  // ポーリング間隔
}

struct Ads1115::Impl {
    II2cDriver* driver;
    bool        owns_driver;  // Impl がドライバを所有しているか（PIMPL + 不透明ポインタの所有権管理）
    uint16_t    addr;
    Ads1115::Gain gain;
    bool        rdy_pin_enabled;

    Impl(const std::string& path, uint16_t a)
        : driver(new I2cDriver(path)), owns_driver(true), addr(a),
          gain(Ads1115::Gain::FSR_2_048V), rdy_pin_enabled(false) {}

    Impl(II2cDriver* drv, uint16_t a)
        : driver(drv), owns_driver(false), addr(a),
          gain(Ads1115::Gain::FSR_2_048V), rdy_pin_enabled(false) {}

    ~Impl() { if (owns_driver) delete driver; }

    Impl(const Impl&)            = delete;
    Impl& operator=(const Impl&) = delete;

    // 16bit レジスタを書き込む（MSB first）
    bool write_reg16(uint8_t reg, uint16_t value) {
        uint8_t buf[3] = { reg,
                           static_cast<uint8_t>(value >> 8),
                           static_cast<uint8_t>(value & 0xFF) };
        return driver->write(buf, sizeof(buf)) == static_cast<int>(sizeof(buf));
    }

    // 16bit レジスタを読み出す（リピーテッドスタート）
    bool read_reg16(uint8_t reg, uint16_t& out) {
        uint8_t rx[2] = {};
        if (driver->write_read(&reg, 1, rx, 2) < 0) return false;
        out = static_cast<uint16_t>((rx[0] << 8) | rx[1]);
        return true;
    }
};

Ads1115::Ads1115(const std::string& i2c_path, uint16_t addr)
    : impl_(std::make_unique<Impl>(i2c_path, addr))
{}

Ads1115::Ads1115(II2cDriver* driver, uint16_t addr)
    : impl_(std::make_unique<Impl>(driver, addr))
{}

Ads1115::~Ads1115() = default;

bool Ads1115::open() noexcept
{
    return impl_->driver->open(impl_->addr);
}

void Ads1115::close() noexcept
{
    impl_->driver->close();
}

bool Ads1115::is_open() const noexcept
{
    return impl_->driver->is_open();
}

void Ads1115::set_gain(Gain g) noexcept
{
    impl_->gain = g;
}

Ads1115::Gain Ads1115::gain() const noexcept
{
    return impl_->gain;
}

double Ads1115::full_scale_volts() const noexcept
{
    return full_scale(impl_->gain);
}

std::optional<int16_t> Ads1115::read_raw(uint8_t channel)
{
    if (channel >= CHANNEL_COUNT) {
        return std::nullopt;
    }

    // シングルショット変換を開始する Config を組み立てる
    uint16_t comp_que = impl_->rdy_pin_enabled ? CFG_COMP_QUE_ONE : CFG_COMP_QUE_DISABLE;
    uint16_t config =
        CFG_OS_SINGLE |
        (CFG_MUX_SINGLE | (static_cast<uint16_t>(channel) << 12)) |
        pga_bits(impl_->gain) |
        CFG_MODE_SINGLE |
        CFG_DR_128SPS |
        comp_que;

    if (!impl_->write_reg16(REG_CONFIG, config)) {
        return std::nullopt;
    }

    // 変換完了まで待つ: Config の OS ビット（bit15）が 1 に戻れば完了。
    //（割り込み駆動にしたい場合は enable_conversion_ready_pin() + GPIO を使う）
    for (int i = 0; i < MAX_CONVERSION_POLLS; ++i) {
        uint16_t cfg = 0;
        if (!impl_->read_reg16(REG_CONFIG, cfg)) {
            return std::nullopt;
        }
        if (cfg & CFG_OS_SINGLE) {
            break;  // 変換完了
        }
        usleep(POLL_INTERVAL_US);
    }

    uint16_t raw = 0;
    if (!impl_->read_reg16(REG_CONVERSION, raw)) {
        return std::nullopt;
    }
    return static_cast<int16_t>(raw);
}

std::optional<double> Ads1115::read_voltage(uint8_t channel)
{
    auto raw = read_raw(channel);
    if (!raw) return std::nullopt;
    // 符号付き 16bit のフルスケールは 32768
    return static_cast<double>(*raw) * full_scale(impl_->gain) / 32768.0;
}

bool Ads1115::enable_conversion_ready_pin() noexcept
{
    // Hi_thresh MSB=1 / Lo_thresh MSB=0 にすると ALERT/RDY が変換完了通知になる
    if (!impl_->write_reg16(REG_HI_THRESH, 0x8000)) return false;
    if (!impl_->write_reg16(REG_LO_THRESH, 0x0000)) return false;
    impl_->rdy_pin_enabled = true;
    return true;
}

} // namespace embedded
