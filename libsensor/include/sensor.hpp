#pragma once
#include "../../spi-hal/include/ispi_driver.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace embedded {

/**
 * @brief MCP3008（8ch / 10bit SPI ADC）の高レベルアクセスクラス
 *
 * SpiDriver を PIMPL イディオムで隠蔽し、MCP3008 のシングルエンドモードの
 * チャネル読み出し API を提供する。
 *
 * - 入力レンジ: 0V 〜 Vref（デフォルト 3.3V）
 * - 分解能: 10 bit（0 〜 1023）
 * - 8 チャネル（CH0 〜 CH7）
 *
 * MCP3008 の通信プロトコル（シングルエンドモード）:
 * @code
 *   TX: [ 0x01,  0x80 | (channel << 4),  0x00 ]
 *   RX: [   _,        ----- 10 bit -----      ]
 *   raw = ((rx[1] & 0x03) << 8) | rx[2];
 * @endcode
 *
 * @note スレッドセーフではない。複数スレッドから使用する場合は呼び出し側で排他制御すること。
 * @note コピー禁止。
 *
 * @code
 * embedded::Sensor sensor("/dev/spidev0.0");
 * if (!sensor.open()) { return; }
 * auto v = sensor.read_voltage(0);   // CH0 の電圧を取得
 * if (v) std::cout << *v << " V\n";
 * @endcode
 */
class Sensor {
public:
    /** @brief MCP3008 のチャネル数 */
    static constexpr uint8_t CHANNEL_COUNT = 8;
    /** @brief MCP3008 の分解能（ADC 最大値）*/
    static constexpr uint16_t ADC_MAX = 1023;
    /** @brief デフォルト Vref [V] */
    static constexpr double DEFAULT_VREF = 3.3;

    /**
     * @brief 非同期読み出し完了コールバック型
     * @param raw  読み出し値（0〜1023）。失敗時は std::nullopt
     * @param err  成功時 0、失敗時は errno 値
     */
    using ReadCallback = std::function<void(std::optional<uint16_t> raw, int err)>;

    /**
     * @brief コンストラクタ（実機用）
     * @param spi_path spidev のデバイスパス（例: "/dev/spidev0.0"）
     * @param vref     Vref [V]（デフォルト 3.3V）
     */
    explicit Sensor(const std::string& spi_path, double vref = DEFAULT_VREF);

    /**
     * @brief コンストラクタ（テスト用）
     *
     * ISpiDriver を外部から差し込むことで実機なしにテストできる。
     * @param driver テスト用ドライバ（MockSpiDriver など）。所有権は渡さない
     * @param vref   Vref [V]（デフォルト 3.3V）
     */
    explicit Sensor(ISpiDriver* driver, double vref = DEFAULT_VREF);

    /** @brief デストラクタ。オープン中なら自動的に close する */
    ~Sensor();

    Sensor(const Sensor&)            = delete;
    Sensor& operator=(const Sensor&) = delete;

    /**
     * @brief デバイスをオープンする
     * @return true: 成功 / false: 失敗
     *
     * **テストケース（UT-LIB-002）** — 無効なパスでは false を返す:
     * @snippet test_sensor.cpp UT-LIB-002
     */
    [[nodiscard]] bool open() noexcept;

    /** @brief デバイスをクローズする */
    void close() noexcept;

    /** @return デバイスがオープン中なら true */
    [[nodiscard]] bool is_open() const noexcept;

    /**
     * @brief 指定チャネルの ADC 生値を読む（10bit, 0〜1023）
     *
     * @param channel チャネル番号（0〜7）
     * @return ADC 値。失敗時は std::nullopt（無効チャネル / 未オープン / 転送失敗）
     *
     * **テストケース（UT-LIB-003）** — MockSpiDriver で raw 値を検証:
     * @snippet test_sensor.cpp UT-LIB-003
     *
     * **テストケース（UT-LIB-004）** — 無効チャネルは std::nullopt:
     * @snippet test_sensor.cpp UT-LIB-004
     */
    [[nodiscard]] std::optional<uint16_t> read_raw(uint8_t channel);

    /**
     * @brief 指定チャネルの電圧を読む [V]
     *
     * @param channel チャネル番号（0〜7）
     * @return 電圧値。失敗時は std::nullopt
     *
     * @note 内部で `read_raw(channel) * vref() / ADC_MAX` を計算する。
     *
     * **テストケース（UT-LIB-005）** — vref に応じて電圧が計算される:
     * @snippet test_sensor.cpp UT-LIB-005
     */
    [[nodiscard]] std::optional<double> read_voltage(uint8_t channel);

    /**
     * @brief 非同期で ADC 生値を読む
     *
     * 内部で std::thread を生成してデタッチする。完了時に cb を呼び出す。
     *
     * @param channel チャネル番号（0〜7）
     * @param cb      完了コールバック
     * @warning Sensor オブジェクトのライフタイムはコールバック完了まで呼び出し側が保証すること
     *
     * **テストケース（UT-LIB-007）** — 未オープン時もコールバックが呼ばれる:
     * @snippet test_sensor.cpp UT-LIB-007
     */
    void read_raw_async(uint8_t channel, ReadCallback cb);

    /** @return 現在の Vref [V] */
    [[nodiscard]] double vref() const noexcept;

    /** @brief Vref [V] を変更する（read_voltage の換算に影響） */
    void set_vref(double vref) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace embedded
