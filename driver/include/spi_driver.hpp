#pragma once
#include "ispi_driver.hpp"
#include <string>

namespace embedded {

/**
 * @brief Linux spidev を介した SPI フルデュプレクス通信ドライバ
 *
 * ISpiDriver を実装した実機向けクラス。
 * /dev/spidevX.Y を open/ioctl/close で管理する。
 * コピー禁止（RAII）。スレッドセーフではない。
 *
 * @code
 * embedded::SpiDriver drv("/dev/spidev0.0");
 * SpiDriver::Config cfg{1000000, 8, 0};
 * if (drv.open(cfg)) {
 *     uint8_t tx[] = {0x80, 0x00};
 *     uint8_t rx[2] = {};
 *     drv.transfer(tx, rx, sizeof(tx));
 * }
 * @endcode
 */
class SpiDriver : public ISpiDriver {
public:
    /**
     * @brief コンストラクタ
     * @param device_path spidev のデバイスパス（例: "/dev/spidev0.0"）
     */
    explicit SpiDriver(const std::string& device_path);

    /** @brief デストラクタ。open 中なら自動的に close する */
    ~SpiDriver() override;

    SpiDriver(const SpiDriver&)            = delete;
    SpiDriver& operator=(const SpiDriver&) = delete;

    /**
     * @brief デバイスをオープンし SPI パラメータを設定する
     * @param cfg 転送速度・ビット幅・モードを指定する Config 構造体
     * @return true: 成功 / false: 失敗（last_errno() で原因を確認）
     */
    bool open(const Config& cfg) override;

    /** @brief デバイスをクローズする。未オープン時は何もしない */
    void close() override;

    /**
     * @brief フルデュプレクス SPI 転送を行う
     *
     * EAGAIN が返った場合は最大 3 回リトライする。
     *
     * @param tx  送信バッファ（len バイト）
     * @param rx  受信バッファ（len バイト）
     * @param len 転送バイト数
     * @return 転送バイト数。エラー時は -1（last_errno() で原因を確認）
     */
    int  transfer(const uint8_t* tx, uint8_t* rx, size_t len) override;

    /** @return デバイスがオープン中なら true */
    bool is_open() const override { return fd_ >= 0; }

    /** @return 直近のエラーの errno 値 */
    int  last_errno() const override { return last_errno_; }

private:
    std::string device_path_;
    int         fd_;
    int         last_errno_;
};

} // namespace embedded
