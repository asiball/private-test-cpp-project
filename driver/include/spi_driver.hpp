#pragma once
#include <cstdint>
#include <string>

namespace embedded {

// SPIデバイスへの低レベルアクセスを抽象化するドライバクラス
class SpiDriver {
public:
    struct Config {
        uint32_t speed_hz;
        uint8_t  bits_per_word;
        uint8_t  mode;           // SPI_MODE_0..3
    };

    explicit SpiDriver(const std::string& device_path);
    ~SpiDriver();

    SpiDriver(const SpiDriver&)            = delete;
    SpiDriver& operator=(const SpiDriver&) = delete;

    bool open(const Config& cfg);
    void close();

    // 戻り値: 送受信バイト数。エラー時は -1
    int  transfer(const uint8_t* tx, uint8_t* rx, size_t len);

    bool is_open() const { return fd_ >= 0; }
    int  last_errno() const { return last_errno_; }

private:
    std::string device_path_;
    int         fd_;
    int         last_errno_;
};

} // namespace embedded
