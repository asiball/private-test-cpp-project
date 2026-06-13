#pragma once
// i2c-hal は別コンポーネント。単純名 include は ads1115.hpp と同じ規約（CLAUDE.md
// 落とし穴 #1）。スタンドアロンテストは -I i2c-hal/include で解決する。
#include "ii2c_driver.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace embedded {

/**
 * @brief MCP9808（I2C 温度センサ）の高レベルアクセスクラス
 *
 * Microchip 製の I2C アンビエント温度センサ。アドレスは ADDR ピンで 0x18〜0x1F に設定可能。
 * `Ads1115` と同じく `II2cDriver` に依存し、PIMPL + 依存注入(DI) で実装を隠蔽する
 * （機能がシンプルな I2C デバイスの最小実装例）。
 *
 * - 分解能: 0.0625 °C/LSB（13bit + 符号）
 * - アンビエント温度レジスタ(0x05)を 2 バイト読むだけで温度が得られる
 *
 * @note スレッドセーフではない。複数スレッドから使う場合は呼び出し側で排他制御すること。
 * @note コピー禁止。
 *
 * @code
 * embedded::Mcp9808 dev("/dev/i2c-1");
 * if (!dev.open()) { return; }            // open 内で製造者 ID を確認
 * if (auto t = dev.read_temperature())
 *     std::cout << *t << " degC\n";
 * @endcode
 */
class Mcp9808 {
public:
    /** @brief デフォルト I2C スレーブアドレス（ADDR ピン = 全て GND）*/
    static constexpr uint16_t DEFAULT_ADDR = 0x18;
    /** @brief 製造者 ID（MANUF_ID, 0x06）の固定値 = Microchip。open 時の疎通確認に使う */
    static constexpr uint16_t MANUFACTURER_ID = 0x0054;

    /**
     * @brief コンストラクタ（実機用）
     * @param i2c_path i2c-dev のデバイスパス（例: "/dev/i2c-1"）
     * @param addr     スレーブアドレス（0x18〜0x1F。デフォルト 0x18）
     */
    explicit Mcp9808(const std::string& i2c_path, uint16_t addr = DEFAULT_ADDR);

    /**
     * @brief コンストラクタ（テスト用）
     * @param driver テスト用ドライバ（MockI2cDriver など）。所有権は渡さない
     * @param addr   スレーブアドレス（デフォルト 0x18）
     */
    explicit Mcp9808(II2cDriver* driver, uint16_t addr = DEFAULT_ADDR);

    /** @brief デストラクタ。オープン中なら自動的に close する */
    ~Mcp9808();

    Mcp9808(const Mcp9808&)            = delete;
    Mcp9808& operator=(const Mcp9808&) = delete;

    /**
     * @brief バスをオープンし、製造者 ID(0x0054) を確認する
     * @return true: 成功 / false: オープン失敗または ID 不一致（誤デバイス検出）
     */
    [[nodiscard]] bool open() noexcept;

    /** @brief バスをクローズする */
    void close() noexcept;

    /** @return オープン中なら true */
    [[nodiscard]] bool is_open() const noexcept;

    /**
     * @brief 製造者 ID（MANUF_ID, 0x06）を読む
     * @return 正常時 0x0054。転送失敗時は std::nullopt
     */
    [[nodiscard]] std::optional<uint16_t> read_manufacturer_id() noexcept;

    /**
     * @brief アンビエント温度レジスタ(0x05)の生値（フラグビット含む 16bit）を読む
     * @return レジスタ値。転送失敗時は std::nullopt
     */
    [[nodiscard]] std::optional<uint16_t> read_temp_raw() noexcept;

    /**
     * @brief アンビエント温度を摂氏で読む
     * @return 温度 [°C]（0.0625 °C 分解能）。転送失敗時は std::nullopt
     */
    [[nodiscard]] std::optional<double> read_temperature() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace embedded
