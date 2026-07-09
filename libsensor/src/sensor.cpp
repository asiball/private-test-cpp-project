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

// MCP3008 の SPI 設定（open() で使用）
constexpr uint32_t MCP3008_SPI_SPEED_HZ      = 1000000;  // 1 MHz（Vdd=3.3V 時の上限は 1.35 MHz）
constexpr uint8_t  MCP3008_SPI_BITS_PER_WORD = 8;
constexpr uint8_t  MCP3008_SPI_MODE          = 0;         // SPI_MODE_0

// MCP3008 シングルエンドモードで 1 回の SPI 転送を行う。
//   TX: [ 0x01,  0x80 | (channel << 4),  0x00 ]
//   RX: [   _,        ----- 10 bit -----      ]
// 呼び出し側で driver に対する排他制御（Impl::io_mutex）を取得済みであること。
std::optional<uint16_t> mcp3008_transfer_locked(ISpiDriver* driver, uint8_t channel) noexcept
{
    uint8_t tx[3] = { MCP3008_START_BIT,
                      static_cast<uint8_t>(MCP3008_SINGLE_MODE | (channel << 4)),
                      0x00 };
    uint8_t rx[3] = { 0, 0, 0 };

    if (driver->transfer(tx, rx, 3) < 0) {
        return std::nullopt;
    }
    return static_cast<uint16_t>((rx[1] & MCP3008_RESULT_MASK) << 8) | rx[2];
}
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

    // driver->transfer() 〜 driver->last_errno() の読み出しまでを直列化する。
    // read_raw_async は複数のワーカースレッドを生成しうるため、同一 ISpiDriver
    // に対して transfer() が並行実行されたり、SpiDriver::last_errno_（非atomic）
    // への並行読み書きが起きるとデータレース（UB）になる。同期版 read_raw との
    // 並行呼び出しも同様のためここで一括して保護する。
    std::mutex                io_mutex;

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
    cfg.speed_hz      = MCP3008_SPI_SPEED_HZ;
    cfg.bits_per_word = MCP3008_SPI_BITS_PER_WORD;
    cfg.mode          = MCP3008_SPI_MODE;
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

    // driver->transfer() を呼ぶ間は io_mutex で排他する。read_raw_async の
    // ワーカースレッドと同一 driver に並行アクセスしても data race にならない。
    std::lock_guard<std::mutex> lk(impl_->io_mutex);
    return mcp3008_transfer_locked(impl_->driver, channel);
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
        // transfer() 呼び出しから last_errno() 読み出しまでを io_mutex 下で行う。
        // read_raw() 経由にすると last_errno() の読み出しがロック区間の外に
        // 出てしまい、別スレッドの transfer() と data race になるため、
        // ここでは mcp3008_transfer_locked を直接同じロック下で呼ぶ。
        std::optional<uint16_t> result;
        int err = 0;
        {
            std::lock_guard<std::mutex> io_lk(impl_->io_mutex);
            result = mcp3008_transfer_locked(impl_->driver, channel);
            if (!result) {
                err = impl_->driver->last_errno();
            }
        }
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
