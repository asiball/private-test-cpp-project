#pragma once
#include <cstdint>
#include <cstddef>

namespace embedded {

/**
 * @brief SPI ドライバの抽象インターフェース
 *
 * このインターフェースを介することで、テスト時に MockSpiDriver に差し替えられる。
 * SpiDriver（実機用）と MockSpiDriver（テスト用）が共通の型として扱われる。
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

    virtual bool open(const Config& cfg) = 0;
    virtual void close() = 0;
    virtual int  transfer(const uint8_t* tx, uint8_t* rx, size_t len) = 0;
    virtual bool is_open() const = 0;
    virtual int  last_errno() const = 0;
};

} // namespace embedded
