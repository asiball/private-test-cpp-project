#include "adxl345.hpp"
#include "ispi_driver.hpp"
#include "spi_driver.hpp"

#include <memory>

namespace embedded {

namespace reg = adxl345::reg;

namespace {
// ADXL345 の SPI 設定（open() で使用）
constexpr uint32_t ADXL345_SPI_SPEED_HZ      = 1000000;  // 1 MHz（SPI 上限 5MHz に対し安全側）
constexpr uint8_t  ADXL345_SPI_BITS_PER_WORD = 8;
constexpr uint8_t  ADXL345_SPI_MODE          = 3;         // SPI_MODE_3 (CPOL=1, CPHA=1)
}

struct Adxl345::Impl {
    std::unique_ptr<ISpiDriver> owned;   // 自前生成時のみ所有（注入時は空）
    ISpiDriver*                 driver;  // 実際に使う非所有ビュー

    explicit Impl(const std::string& path)
        : owned(std::make_unique<SpiDriver>(path)), driver(owned.get()) {}

    explicit Impl(ISpiDriver* drv)
        : driver(drv) {}

    // owned (unique_ptr) によりデストラクタで自動解放。手動 delete / owns_driver は不要。
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
    cfg.speed_hz      = ADXL345_SPI_SPEED_HZ;
    cfg.bits_per_word = ADXL345_SPI_BITS_PER_WORD;
    cfg.mode          = ADXL345_SPI_MODE;
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

std::optional<uint8_t> Adxl345::read_reg(uint8_t addr) noexcept
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

bool Adxl345::write_reg(uint8_t addr, uint8_t value) noexcept
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
bool Adxl345::update_bits(uint8_t addr, uint8_t mask, uint8_t value) noexcept
{
    auto current = read_reg(addr);
    if (!current) {
        return false;
    }
    uint8_t updated = static_cast<uint8_t>((*current & ~mask) | (value & mask));
    return write_reg(addr, updated);
}

std::optional<uint8_t> Adxl345::read_device_id() noexcept
{
    return read_reg(reg::DEVID);
}

std::optional<Adxl345::AccelRaw> Adxl345::read_raw() noexcept
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

std::optional<Adxl345::AccelG> Adxl345::read_g() noexcept
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

bool Adxl345::enable_tap_detection(uint8_t threshold, uint8_t duration, uint8_t axes) noexcept
{
    // ヘッダの規定どおり threshold / duration の 0 は不可（0 は「検出しない」設定に
    // 相当し意図しない誤設定であるため、ここで弾く）。
    if (threshold == 0 || duration == 0) return false;
    // 閾値・持続時間・対象軸を設定 → SINGLE_TAP を INT1 にマップ → 有効化。
    // どのレジスタをどの順序で触るかをここに隠蔽する（利用者のレジスタ直叩きを不要にする）。
    if (!write_reg(reg::THRESH_TAP, threshold)) return false;
    if (!write_reg(reg::DUR, duration))         return false;
    if (!write_reg(reg::TAP_AXES, axes))        return false;
    // INT_MAP のビット 0 = INT1 ピンへ出力。SINGLE_TAP を INT1 に割り当てる。
    if (!update_bits(reg::INT_MAP, adxl345::int_bits::SINGLE_TAP, 0)) return false;
    // INT_ENABLE で SINGLE_TAP を有効化（他の割り込みは保持）
    return update_bits(reg::INT_ENABLE, adxl345::int_bits::SINGLE_TAP,
                       adxl345::int_bits::SINGLE_TAP);
}

bool Adxl345::enable_free_fall(uint8_t threshold, uint8_t time) noexcept
{
    // ヘッダの規定どおり threshold / time の 0 は不可（tap detection と同様、
    // 0 は「検出しない」設定に相当し意図しない誤設定であるため弾く）。
    if (threshold == 0 || time == 0) return false;
    if (!write_reg(reg::THRESH_FF, threshold)) return false;
    if (!write_reg(reg::TIME_FF, time))        return false;
    if (!update_bits(reg::INT_MAP, adxl345::int_bits::FREE_FALL, 0)) return false;
    return update_bits(reg::INT_ENABLE, adxl345::int_bits::FREE_FALL,
                       adxl345::int_bits::FREE_FALL);
}

bool Adxl345::disable_interrupts() noexcept
{
    return write_reg(reg::INT_ENABLE, 0x00);
}

std::optional<uint8_t> Adxl345::read_interrupt_source() noexcept
{
    return read_reg(reg::INT_SOURCE);
}

} // namespace embedded
