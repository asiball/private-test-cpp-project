#include "sensor.hpp"
#include "ispi_driver.hpp"
#include "spi_driver.hpp"

#include <cerrno>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace embedded {

namespace {
// MCP3008 シングルエンドモードのコマンドバイト
constexpr uint8_t MCP3008_START_BIT   = 0x01;  // スタートビット
constexpr uint8_t MCP3008_SINGLE_MODE = 0x80;  // SGL/DIFF=1 (single-ended)
constexpr uint8_t MCP3008_RESULT_MASK = 0x03;  // rx[1] の上位 2bit (10bit の bit9-8)
}

struct Sensor::Impl {
    std::unique_ptr<ISpiDriver> owned;   // 自前生成時のみ所有（注入時は空）
    ISpiDriver*                 driver;  // 実際に使う非所有ビュー
    double                      vref_volts;

    // read_raw_async が生成したワーカースレッド。detach せずここに保持し、
    // デストラクタで join することで「コールバック完了前に Sensor が破棄されて
    // this がダングリングになる」UAF を構造的に防ぐ（Impl 破棄まで Sensor は生存）。
    std::mutex                workers_mtx;
    std::vector<std::thread>  workers;

    explicit Impl(const std::string& path, double v)
        : owned(std::make_unique<SpiDriver>(path)), driver(owned.get()), vref_volts(v) {}

    explicit Impl(ISpiDriver* drv, double v)
        : driver(drv), vref_volts(v) {}

    ~Impl() {
        // 走行中のワーカーを全て待ち合わせてから driver / owned を破棄する。
        // join 中も Impl のメンバ（driver）は生存しているためコールバックは安全。
        for (auto& t : workers) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

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

std::optional<uint16_t> Sensor::read_raw(uint8_t channel) noexcept
{
    if (channel >= CHANNEL_COUNT) {
        return std::nullopt;
    }

    // MCP3008 シングルエンドモード:
    //   TX: [ 0x01,  0x80 | (channel << 4),  0x00 ]
    //   RX: [   _,        ----- 10 bit -----      ]
    uint8_t tx[3] = { MCP3008_START_BIT,
                      static_cast<uint8_t>(MCP3008_SINGLE_MODE | (channel << 4)),
                      0x00 };
    uint8_t rx[3] = { 0, 0, 0 };

    if (impl_->driver->transfer(tx, rx, 3) < 0) {
        return std::nullopt;
    }
    uint16_t raw = static_cast<uint16_t>((rx[1] & MCP3008_RESULT_MASK) << 8) | rx[2];
    return raw;
}

std::optional<double> Sensor::read_voltage(uint8_t channel) noexcept
{
    auto raw = read_raw(channel);
    if (!raw) return std::nullopt;
    return static_cast<double>(*raw) * impl_->vref_volts / ADC_MAX;
}

void Sensor::read_raw_async(uint8_t channel, ReadCallback cb)
{
    // 無効チャネルはスレッドを起こすまでもなく即時失敗を通知する。
    // （ドライバの last_errno() は更新されないため、ここで EINVAL を明示する。
    //  さもないと「以前の無関係な errno」や 0 が渡り、契約 (nullopt なら errno) と矛盾する）
    if (channel >= CHANNEL_COUNT) {
        cb(std::nullopt, EINVAL);
        return;
    }
    // ワーカーは detach せず impl_->workers に保持し、~Impl で join する。
    // これにより Sensor 破棄時にコールバック完了を待ち合わせ、this のダングリング
    // （UAF）を防ぐ。長期稼働デーモンで多数発行する場合は完了済みスレッドが
    // 蓄積しうる点に注意（用途に応じ呼び出し側で間引く）。
    std::lock_guard<std::mutex> lk(impl_->workers_mtx);
    impl_->workers.emplace_back([this, channel, cb]() {
        auto result = this->read_raw(channel);
        int  err    = result ? 0 : impl_->driver->last_errno();
        cb(result, err);
    });
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
