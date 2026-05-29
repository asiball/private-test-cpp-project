#pragma once
#include "ispi_driver.hpp"

#include <string>

namespace embedded {

/**
 * @brief my_spi_driver カーネルモジュールを介した SPI 通信ドライバ（ユーザー空間側）
 *
 * /dev/my_spi_dev を open/ioctl/close で操作する ISpiDriver 実装。
 * カーネルモジュール my_spi_driver.ko がロード済みであることが前提。
 * SpiDriver（spidev ラッパー）と異なり、独自 ioctl を使用する。
 *
 * コピー禁止（RAII）。スレッドセーフではない。
 *
 * @code
 * // 事前に: sudo insmod my_spi_driver.ko
 * embedded::KernelSpiDriver drv("/dev/my_spi_dev");
 * KernelSpiDriver::Config cfg{1000000, 8, 0};
 * if (drv.open(cfg)) {
 *     uint8_t tx[] = {0x80, 0x00};
 *     uint8_t rx[2] = {};
 *     drv.transfer(tx, rx, sizeof(tx));
 * }
 * @endcode
 */
class KernelSpiDriver : public ISpiDriver {
public:
    /**
     * @brief コンストラクタ
     * @param device_path デバイスパス（デフォルト: "/dev/my_spi_dev"）
     */
    explicit KernelSpiDriver(const std::string& device_path = "/dev/my_spi_dev");

    /** @brief デストラクタ。open 中なら自動的に close する */
    ~KernelSpiDriver() override;

    KernelSpiDriver(const KernelSpiDriver&)            = delete;
    KernelSpiDriver& operator=(const KernelSpiDriver&) = delete;

    /**
     * @brief デバイスをオープンし SPI パラメータを設定する
     *
     * MY_SPI_IOC_CONFIG ioctl でカーネルドライバに設定を送る。
     *
     * @param cfg 転送速度・ビット幅・モードを指定する Config 構造体
     * @return true: 成功 / false: 失敗（last_errno() で原因を確認）
     */
    [[nodiscard]] bool open(const Config& cfg) noexcept override;

    /** @brief デバイスをクローズする。未オープン時は何もしない */
    void close() noexcept override;

    /**
     * @brief フルデュプレクス SPI 転送を行う
     *
     * MY_SPI_IOC_TRANSFER ioctl でカーネルドライバに転送を依頼する。
     *
     * @param tx  送信バッファ（len バイト）
     * @param rx  受信バッファ（len バイト）
     * @param len 転送バイト数（最大 4096）
     * @return 転送バイト数。エラー時は -1（last_errno() で原因を確認）
     */
    [[nodiscard]] int transfer(const uint8_t* tx, uint8_t* rx, size_t len) noexcept override;

    /** @return デバイスがオープン中なら true */
    [[nodiscard]] bool is_open() const noexcept override { return fd_ >= 0; }

    /** @return 直近のエラーの errno 値 */
    [[nodiscard]] int last_errno() const noexcept override { return last_errno_; }

private:
    std::string device_path_;
    int         fd_         = -1;
    int         last_errno_ = 0;
};

} // namespace embedded
