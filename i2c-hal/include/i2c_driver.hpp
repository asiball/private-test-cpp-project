#pragma once
#include "ii2c_driver.hpp"

#include <string>

namespace embedded {

/**
 * @brief Linux i2c-dev を介した I2C 通信ドライバ（実機用）
 *
 * II2cDriver を実装した実機向けクラス。/dev/i2c-N を open し、
 * ioctl(I2C_SLAVE) でスレーブアドレスを設定して read/write する。
 * コピー禁止（RAII）。スレッドセーフではない。
 *
 * spi-hal の SpiDriver と対になる存在で、構造（コンストラクタでパス保持、
 * open でデバイス確保、デストラクタで close）も意図的に揃えてある。
 *
 * @code
 * embedded::I2cDriver bus("/dev/i2c-1");
 * if (bus.open(0x48)) {                 // ADS1115
 *     uint8_t reg = 0x00;               // Conversion レジスタ
 *     uint8_t rx[2] = {};
 *     bus.write_read(&reg, 1, rx, 2);
 * }
 * @endcode
 */
class I2cDriver : public II2cDriver {
public:
    /**
     * @brief コンストラクタ
     * @param device_path i2c-dev のデバイスパス（例: "/dev/i2c-1"）
     */
    explicit I2cDriver(const std::string& device_path);

    /** @brief デストラクタ。open 中なら自動的に close する */
    ~I2cDriver() override;

    I2cDriver(const I2cDriver&)            = delete;
    I2cDriver& operator=(const I2cDriver&) = delete;

    [[nodiscard]] bool open(uint16_t addr) noexcept override;
    void close() noexcept override;
    [[nodiscard]] int write(const uint8_t* data, size_t len) noexcept override;
    [[nodiscard]] int read(uint8_t* data, size_t len) noexcept override;
    [[nodiscard]] int write_read(const uint8_t* tx, size_t tx_len,
                                 uint8_t* rx, size_t rx_len) noexcept override;
    [[nodiscard]] bool is_open() const noexcept override { return fd_ >= 0; }
    [[nodiscard]] int  last_errno() const noexcept override { return last_errno_; }

private:
    std::string device_path_;
    uint16_t    addr_;
    int         fd_;
    int         last_errno_;
};

} // namespace embedded
