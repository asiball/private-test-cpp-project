#pragma once
#include <cstddef>
#include <cstdint>

namespace embedded {

/**
 * @brief SPI ドライバの抽象インターフェース
 *
 * SpiDriver（実機用）と MockSpiDriver（テスト用）が共通の型として扱われる。
 * このインターフェースを介することで、Sensor クラスが実機に依存しなくなる。
 */
class ISpiDriver {
public:
    /** @brief SPI 設定パラメータ */
    struct Config {
        uint32_t speed_hz;      ///< クロック周波数 [Hz]
        uint8_t  bits_per_word; ///< ワードビット幅（通常 8）
        uint8_t  mode;          ///< SPI モード 0〜3
    };

    virtual ~ISpiDriver() = default;

    /**
     * @brief デバイスをオープンし SPI パラメータを設定する
     * @return true: 成功 / false: 失敗
     * @note 戻り値の確認を必須とする（[[nodiscard]]）
     *
     * **テストケース（UT-DRV-002）** — 無効なパスでは false を返す:
     * @snippet test_spi_driver.cpp UT-DRV-002
     */
    [[nodiscard]] virtual bool open(const Config& cfg) noexcept = 0;

    /**
     * @brief デバイスをクローズする。未オープン時は no-op
     *
     * **テストケース（UT-DRV-004）** — 二重 close は安全:
     * @snippet test_spi_driver.cpp UT-DRV-004
     */
    virtual void close() noexcept = 0;

    /**
     * @brief フルデュプレクス SPI 転送を行う
     * @return 転送バイト数。エラー時は -1
     * @note 戻り値の確認を必須とする（[[nodiscard]]）
     *
     * **テストケース（UT-DRV-005）** — 未オープン時は -1 を返す:
     * @snippet test_spi_driver.cpp UT-DRV-005
     */
    [[nodiscard]] virtual int transfer(const uint8_t* tx, uint8_t* rx, size_t len) noexcept = 0;

    /** @return デバイスがオープン中なら true */
    [[nodiscard]] virtual bool is_open() const noexcept = 0;

    /** @return 直近のエラーの errno 値 */
    [[nodiscard]] virtual int  last_errno() const noexcept = 0;
};

} // namespace embedded
