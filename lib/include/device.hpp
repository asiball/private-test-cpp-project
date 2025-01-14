#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace embedded {

// ユーザ空間からデバイスを操作するための高レベルAPIを提供する
class Device {
public:
    // v1.1.0 で追加: 非同期読み出し完了コールバック
    using ReadCallback = std::function<void(const std::vector<uint8_t>&, int /*errno*/)>;

    explicit Device(const std::string& spi_path);
    ~Device();

    Device(const Device&)            = delete;
    Device& operator=(const Device&) = delete;

    bool open();
    void close();

    // 同期読み出し
    std::vector<uint8_t> read(uint8_t reg, size_t len);

    // 同期書き込み
    bool write(uint8_t reg, const std::vector<uint8_t>& data);

    // v1.1.0: 非同期読み出し（別スレッドで実行、完了時にcbを呼ぶ）
    void read_async(uint8_t reg, size_t len, ReadCallback cb);

    bool is_open() const;

private:
    struct Impl;
    Impl* impl_;  // PIMPLイディオムでドライバ依存を隠蔽
};

} // namespace embedded
