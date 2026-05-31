#pragma once
#include "../../spi-hal/include/ispi_driver.hpp"
#include "adxl345_reg.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace embedded {

/**
 * @brief ADXL345（3軸 / 13bit SPI 加速度センサ）の高レベルアクセスクラス
 *
 * SpiDriver を PIMPL イディオムで隠蔽し、レジスタアクセス層
 * （read_reg / write_reg / update_bits）の上に加速度読み出し API を提供する。
 *
 * MCP3008（レジスタ無しのコマンド型 ADC）とは対照的に、ADXL345 は
 * レジスタマップ（@ref adxl345_reg.hpp）を持ち、設定をビット単位で行う。
 *
 * - SPI モード: MODE 3（CPOL=1, CPHA=1）
 * - 既定設定: フル分解能（3.9 mg/LSB）+ ±16g レンジ + 測定開始
 *
 * @note スレッドセーフではない。複数スレッドから使用する場合は呼び出し側で排他制御すること。
 * @note コピー禁止。
 *
 * @code
 * embedded::Adxl345 dev("/dev/spidev0.0");
 * if (!dev.open()) { return; }            // open 内で DEVID 確認 + 設定 + 測定開始
 * if (auto a = dev.read_g())
 *     std::cout << a->x << ", " << a->y << ", " << a->z << " g\n";
 * @endcode
 */
class Adxl345 {
public:
    /** @brief フル分解能時のスケール係数 [g/LSB]（3.9 mg/LSB） */
    static constexpr double SCALE_G_PER_LSB = 0.0039;

    /** @brief 3 軸の生値（符号付き 16bit） */
    struct AccelRaw { int16_t x; int16_t y; int16_t z; };

    /** @brief 3 軸の加速度 [g] */
    struct AccelG { double x; double y; double z; };

    /**
     * @brief コンストラクタ（実機用）
     * @param spi_path spidev のデバイスパス（例: "/dev/spidev0.0"）
     */
    explicit Adxl345(const std::string& spi_path);

    /**
     * @brief コンストラクタ（テスト用）
     *
     * ISpiDriver を外部から差し込むことで実機なしにテストできる。
     * @param driver テスト用ドライバ（MockSpiDriver など）。所有権は渡さない
     */
    explicit Adxl345(ISpiDriver* driver);

    /** @brief デストラクタ。オープン中なら自動的に close する */
    ~Adxl345();

    Adxl345(const Adxl345&)            = delete;
    Adxl345& operator=(const Adxl345&) = delete;

    /**
     * @brief デバイスをオープンし、DEVID 確認・初期設定・測定開始まで行う
     *
     * SPI を MODE 3 でオープン → DEVID(0x00) が 0xE5 か確認 →
     * DATA_FORMAT に FULL_RES|±16g を設定 → POWER_CTL の MEASURE を立てる。
     * いずれかに失敗した場合は close して false を返す。
     *
     * @return true: 成功 / false: 失敗（DEVID 不一致・転送失敗など）
     */
    [[nodiscard]] bool open() noexcept;

    /** @brief デバイスをクローズする */
    void close() noexcept;

    /** @return デバイスがオープン中なら true */
    [[nodiscard]] bool is_open() const noexcept;

    // ── レジスタアクセス層 ────────────────────────────────
    // 生のレジスタ操作。デバイス固有の高レベル API はこの上に構築される。

    /**
     * @brief 1 バイトレジスタを読む
     * @param addr レジスタアドレス（0x00〜0x3F）
     * @return レジスタ値。転送失敗時は std::nullopt
     */
    [[nodiscard]] std::optional<uint8_t> read_reg(uint8_t addr);

    /**
     * @brief 1 バイトレジスタへ書く
     * @param addr  レジスタアドレス（0x00〜0x3F）
     * @param value 書き込む値
     * @return true: 成功 / false: 転送失敗
     */
    [[nodiscard]] bool write_reg(uint8_t addr, uint8_t value);

    /**
     * @brief レジスタの一部ビットだけを read-modify-write で更新する
     * @param addr  レジスタアドレス
     * @param mask  更新対象ビット（1 のビットだけ書き換える）
     * @param value 設定値（mask の範囲のみ反映）
     * @return true: 成功 / false: 読み or 書きの転送失敗
     */
    [[nodiscard]] bool update_bits(uint8_t addr, uint8_t mask, uint8_t value);

    // ── 高レベル API ──────────────────────────────────────

    /**
     * @brief デバイス ID（DEVID, 0x00）を読む
     * @return 正常時 0xE5。転送失敗時は std::nullopt
     */
    [[nodiscard]] std::optional<uint8_t> read_device_id();

    /**
     * @brief 3 軸の生値を一括（マルチバイト）読み出す
     * @return 各軸の符号付き 16bit 値。転送失敗時は std::nullopt
     */
    [[nodiscard]] std::optional<AccelRaw> read_raw();

    /**
     * @brief 3 軸の加速度 [g] を読み出す
     * @return 各軸 [g]。転送失敗時は std::nullopt
     * @note 内部で read_raw() の各値に SCALE_G_PER_LSB を掛ける。
     */
    [[nodiscard]] std::optional<AccelG> read_g();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace embedded
