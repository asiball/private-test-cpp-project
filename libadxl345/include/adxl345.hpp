#pragma once
// spi-hal は別コンポーネント。相対パスで直接指すのは、各テストを「インストール済み
// ライブラリ名でリンクするスタンドアロン configure」方式でビルドするため（CLAUDE.md
// 落とし穴 #1）。この相対 include なら -I を足さずに ISpiDriver を解決できる。
// モノレポビルドでは CMake の target_include_directories でも解決される（二重に安全）。
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
 * レジスタマップ（@ref adxl345_reg.hpp に定義）を持ち、設定をビット単位で行う。
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

    // ── 割り込みソースのビットマスク（read_interrupt_source() の戻り値解釈用）──
    static constexpr uint8_t INT_DATA_READY = 0x80; ///< データレディ
    static constexpr uint8_t INT_SINGLE_TAP = 0x40; ///< シングルタップ
    static constexpr uint8_t INT_DOUBLE_TAP = 0x20; ///< ダブルタップ
    static constexpr uint8_t INT_FREE_FALL  = 0x04; ///< 自由落下

    // ── タップ検出に使う軸（enable_tap_detection の axes 引数）──
    static constexpr uint8_t TAP_AXIS_X   = 0x04;
    static constexpr uint8_t TAP_AXIS_Y   = 0x02;
    static constexpr uint8_t TAP_AXIS_Z   = 0x01;
    static constexpr uint8_t TAP_AXIS_XYZ = 0x07; ///< 全軸

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
     *
     * **テストケース（UT-ADXL-008）** — DEVID 一致で DATA_FORMAT/POWER_CTL を設定:
     * @snippet test_adxl345.cpp UT-ADXL-008
     *
     * **テストケース（UT-ADXL-009）** — DEVID 不一致なら false（誤デバイス検出）:
     * @snippet test_adxl345.cpp UT-ADXL-009
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
     *
     * **テストケース（UT-ADXL-003）** — READ ビット付きアドレスを送り rx[1] を返す:
     * @snippet test_adxl345.cpp UT-ADXL-003
     */
    [[nodiscard]] std::optional<uint8_t> read_reg(uint8_t addr) noexcept;

    /**
     * @brief 1 バイトレジスタへ書く
     * @param addr  レジスタアドレス（0x00〜0x3F）
     * @param value 書き込む値
     * @return true: 成功 / false: 転送失敗
     *
     * **テストケース（UT-ADXL-004）** — R/W=0 でアドレスと値を送る:
     * @snippet test_adxl345.cpp UT-ADXL-004
     */
    [[nodiscard]] bool write_reg(uint8_t addr, uint8_t value) noexcept;

    /**
     * @brief レジスタの一部ビットだけを read-modify-write で更新する
     * @param addr  レジスタアドレス
     * @param mask  更新対象ビット（1 のビットだけ書き換える）
     * @param value 設定値（mask の範囲のみ反映）
     * @return true: 成功 / false: 読み or 書きの転送失敗
     *
     * **テストケース（UT-ADXL-005）** — read-modify-write で対象外ビットを保持:
     * @snippet test_adxl345.cpp UT-ADXL-005
     */
    [[nodiscard]] bool update_bits(uint8_t addr, uint8_t mask, uint8_t value) noexcept;

    // ── 高レベル API ──────────────────────────────────────

    /**
     * @brief デバイス ID（DEVID, 0x00）を読む
     * @return 正常時 0xE5。転送失敗時は std::nullopt
     */
    [[nodiscard]] std::optional<uint8_t> read_device_id() noexcept;

    /**
     * @brief 3 軸の生値を一括（マルチバイト）読み出す
     * @return 各軸の符号付き 16bit 値。転送失敗時は std::nullopt
     *
     * **テストケース（UT-ADXL-006）** — DATAX0 からのマルチバイト読みをリトルエンディアン合成:
     * @snippet test_adxl345.cpp UT-ADXL-006
     */
    [[nodiscard]] std::optional<AccelRaw> read_raw() noexcept;

    /**
     * @brief 3 軸の加速度 [g] を読み出す
     * @return 各軸 [g]。転送失敗時は std::nullopt
     * @note 内部で read_raw() の各値に SCALE_G_PER_LSB を掛ける。
     *
     * **テストケース（UT-ADXL-007）** — 3.9mg/LSB でスケールする:
     * @snippet test_adxl345.cpp UT-ADXL-007
     */
    [[nodiscard]] std::optional<AccelG> read_g() noexcept;

    // ── 割り込み API ──────────────────────────────────────
    // ADXL345 はタップ/自由落下等を INT1/INT2 ピンに出力できる。GPIO 割り込み
    // （gpio ライブラリの GpioLine::wait_event）と組み合わせると、ポーリングせず
    // 「割り込みで起こされてから read_interrupt_source() で要因を判別」できる。
    // どのレジスタをどの順序で設定するかをライブラリ側に隠蔽する。

    /**
     * @brief シングルタップ割り込みを設定・有効化する（INT1 にマップ）
     * @param threshold タップ閾値（THRESH_TAP, 62.5 mg/LSB）。0 は不可
     * @param duration  タップとみなす最大持続時間（DUR, 625 us/LSB）。0 は不可
     * @param axes      検出に使う軸（TAP_AXIS_X|Y|Z の OR。既定は全軸）
     * @return true: 全レジスタ設定成功 / false: 転送失敗
     */
    [[nodiscard]] bool enable_tap_detection(uint8_t threshold, uint8_t duration,
                                            uint8_t axes = TAP_AXIS_XYZ) noexcept;

    /**
     * @brief 自由落下割り込みを設定・有効化する（INT1 にマップ）
     * @param threshold 自由落下閾値（THRESH_FF, 62.5 mg/LSB。推奨 0x05〜0x09）
     * @param time      自由落下時間（TIME_FF, 5 ms/LSB。推奨 0x14〜0x46）
     * @return true: 成功 / false: 転送失敗
     */
    [[nodiscard]] bool enable_free_fall(uint8_t threshold, uint8_t time) noexcept;

    /**
     * @brief すべての割り込みを無効化する（INT_ENABLE = 0）
     * @return true: 成功 / false: 転送失敗
     */
    [[nodiscard]] bool disable_interrupts() noexcept;

    /**
     * @brief 割り込みソース（INT_SOURCE, 0x30）を読む
     *
     * 戻り値のビットを @ref INT_SINGLE_TAP / @ref INT_FREE_FALL 等と AND して要因を
     * 判別する。INT_SOURCE はデータ系を除き読み出しでクリアされる。
     * @return INT_SOURCE の値。転送失敗時は std::nullopt
     */
    [[nodiscard]] std::optional<uint8_t> read_interrupt_source() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace embedded
