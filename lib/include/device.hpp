#pragma once
#include "../../driver/include/ispi_driver.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace embedded {

/**
 * @brief SPI デバイスへの高レベルアクセスを提供するクラス
 *
 * SpiDriver を PIMPL イディオムで隠蔽し、レジスタ単位の読み書き API を提供する。
 * 同期 (read/write) と非同期 (read_async) の両方をサポートする。
 *
 * @note スレッドセーフではない。複数スレッドから使用する場合は呼び出し側で排他制御すること。
 * @note コピー禁止。
 *
 * @code
 * embedded::Device dev("/dev/spidev0.0");
 * if (!dev.open()) { return; }
 * auto data = dev.read(0x00, 4);   // 同期読み出し
 *
 * // 非同期読み出し (v1.1.0)
 * dev.read_async(0x00, 4, [](const std::vector<uint8_t>& d, int err) {
 *     if (!err) { for (auto b : d) printf("%02x ", b); }
 * });
 * @endcode
 */
class Device {
public:
    /**
     * @brief 非同期読み出し完了コールバック型 (v1.1.0)
     * @param data  読み出したデータ。エラー時は空 vector
     * @param err   成功時 0、失敗時は errno 値
     */
    using ReadCallback = std::function<void(const std::vector<uint8_t>&, int)>;

    /**
     * @brief コンストラクタ（実機用）
     * @param spi_path spidev のデバイスパス（例: "/dev/spidev0.0"）
     */
    explicit Device(const std::string& spi_path);

    /**
     * @brief コンストラクタ（テスト用）
     *
     * ISpiDriver を外部から差し込むことで実機なしにテストできる。
     * @param driver テスト用ドライバ（MockSpiDriver など）。所有権は渡さない
     */
    explicit Device(ISpiDriver* driver);

    /** @brief デストラクタ。オープン中なら自動的に close する */
    ~Device();

    Device(const Device&)            = delete;
    Device& operator=(const Device&) = delete;

    /**
     * @brief デバイスをオープンする
     * @return true: 成功 / false: 失敗
     *
     * **テストケース（UT-LIB-002）** — 無効なパスでは false を返す:
     * @snippet test_device.cpp UT-LIB-002
     */
    [[nodiscard]] bool open() noexcept;

    /** @brief デバイスをクローズする */
    void close() noexcept;

    /**
     * @brief レジスタから同期読み出し
     *
     * @param reg 読み出し元レジスタアドレス（アドレスビット7 は読み出しフラグとして設定される）
     * @param len 読み出しバイト数
     * @return 読み出したデータ。失敗時は空 vector
     *
     * **テストケース（UT-LIB-003）** — MockSpiDriver でデータ取得を検証:
     * @snippet test_device.cpp UT-LIB-003
     *
     * **テストケース（UT-LIB-004）** — 未オープン時は空 vector:
     * @snippet test_device.cpp UT-LIB-004
     */
    [[nodiscard]] std::vector<uint8_t> read(uint8_t reg, size_t len);

    /**
     * @brief レジスタへ同期書き込み
     * @param reg  書き込み先レジスタアドレス
     * @param data 書き込むバイト列
     * @return true: 成功 / false: 失敗
     *
     * **テストケース（UT-LIB-005）** — MockSpiDriver で成功を検証:
     * @snippet test_device.cpp UT-LIB-005
     *
     * **テストケース（UT-LIB-006）** — 未オープン時は false:
     * @snippet test_device.cpp UT-LIB-006
     */
    [[nodiscard]] bool write(uint8_t reg, const std::vector<uint8_t>& data);

    /**
     * @brief レジスタから非同期読み出し (v1.1.0)
     *
     * 内部で std::thread を生成してデタッチする。
     * 完了時に cb を呼び出す。
     *
     * @param reg 読み出し元レジスタアドレス
     * @param len 読み出しバイト数
     * @param cb  完了コールバック
     * @warning Device オブジェクトのライフタイムはコールバック完了まで呼び出し側が保証すること
     */
    void read_async(uint8_t reg, size_t len, ReadCallback cb);

    /** @return デバイスがオープン中なら true */
    [[nodiscard]] bool is_open() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace embedded
