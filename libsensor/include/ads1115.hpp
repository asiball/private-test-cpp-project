#pragma once
#include "ii2c_driver.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace embedded {

/**
 * @brief ADS1115（4ch / 16bit I2C ADC）の高レベルアクセスクラス
 *
 * 既存の Sensor（MCP3008 = SPI の 10bit ADC）と「同じ ADC という仕事を、別のバスと
 * 別の分解能で」行う対の教材。Sensor が ISpiDriver に依存するのと同様、Ads1115 は
 * II2cDriver に依存し、PIMPL で実装を隠蔽する。
 *
 * @par C 経験者向けのメモ（PIMPL）
 * `struct Impl;` を前方宣言し、本体はソース側にだけ書く。これは C でよくやる
 * 「不透明ポインタ（opaque pointer）」とまったく同じ発想:
 * @code
 *   // C 版: ヘッダには中身を見せず、ポインタだけ公開する
 *   typedef struct ads1115 ads1115_t;     // 中身は .c に隠す
 *   ads1115_t* ads1115_create(const char* path);
 *   void       ads1115_destroy(ads1115_t*);
 * @endcode
 * C++ では `std::unique_ptr<Impl>` が `ads1115_destroy` 相当を自動で呼ぶ（RAII）。
 * ヘッダの再コンパイルを減らし ABI を安定させる狙いも C 版と同じ。
 *
 * - 入力レンジ: PGA（ゲイン）設定に依存。デフォルト ±2.048V
 * - 分解能: 16bit（符号付き、単電源シングルエンドでは実質 0〜32767）
 * - 4 チャネル（A0〜A3, シングルエンド）
 *
 * @note スレッドセーフではない。複数スレッドから使う場合は呼び出し側で排他制御すること。
 * @note コピー禁止。
 */
class Ads1115 {
public:
    /** @brief デフォルト I2C スレーブアドレス（ADDR ピン = GND）*/
    static constexpr uint16_t DEFAULT_ADDR = 0x48;
    /** @brief シングルエンドのチャネル数 */
    static constexpr uint8_t CHANNEL_COUNT = 4;

    /** @brief PGA（プログラマブルゲイン）= 入力フルスケール電圧 */
    enum class Gain : uint8_t {
        FSR_6_144V = 0,  ///< ±6.144V
        FSR_4_096V = 1,  ///< ±4.096V
        FSR_2_048V = 2,  ///< ±2.048V（デフォルト）
        FSR_1_024V = 3,  ///< ±1.024V
        FSR_0_512V = 4,  ///< ±0.512V
        FSR_0_256V = 5,  ///< ±0.256V
    };

    /**
     * @brief コンストラクタ（実機用）
     * @param i2c_path i2c-dev のデバイスパス（例: "/dev/i2c-1"）
     * @param addr     スレーブアドレス（デフォルト 0x48）
     */
    explicit Ads1115(const std::string& i2c_path, uint16_t addr = DEFAULT_ADDR);

    /**
     * @brief コンストラクタ（テスト用）
     * @param driver テスト用ドライバ（MockI2cDriver など）。所有権は渡さない
     * @param addr   スレーブアドレス（デフォルト 0x48）
     */
    explicit Ads1115(II2cDriver* driver, uint16_t addr = DEFAULT_ADDR);

    /** @brief デストラクタ。オープン中なら自動的に close する */
    ~Ads1115();

    Ads1115(const Ads1115&)            = delete;
    Ads1115& operator=(const Ads1115&) = delete;

    /** @brief バスをオープンする @return true: 成功 */
    [[nodiscard]] bool open() noexcept;

    /** @brief バスをクローズする */
    void close() noexcept;

    /** @return オープン中なら true */
    [[nodiscard]] bool is_open() const noexcept;

    /** @brief ゲイン（入力レンジ）を設定する */
    void set_gain(Gain g) noexcept;

    /** @return 現在のゲイン */
    [[nodiscard]] Gain gain() const noexcept;

    /** @return 現在のゲインでのフルスケール電圧 [V] */
    [[nodiscard]] double full_scale_volts() const noexcept;

    /**
     * @brief 指定チャネルをシングルショット変換して生値（符号付き 16bit）を読む
     * @param channel チャネル番号（0〜3）
     * @return ADC 値。失敗時は std::nullopt（無効チャネル / 未オープン / 転送失敗）
     */
    [[nodiscard]] std::optional<int16_t> read_raw(uint8_t channel);

    /**
     * @brief 指定チャネルの電圧を読む [V]
     * @param channel チャネル番号（0〜3）
     * @return 電圧値（`raw * full_scale_volts() / 32768`）。失敗時は std::nullopt
     */
    [[nodiscard]] std::optional<double> read_voltage(uint8_t channel);

    /**
     * @brief ALERT/RDY ピンを「変換完了通知（RDY）」として有効化する
     *
     * Hi_thresh の MSB=1 / Lo_thresh の MSB=0 を書き込み、以降の変換完了時に
     * ALERT/RDY ピンがパルスするよう構成する。GPIO 割り込み（gpio ライブラリ）と
     * 組み合わせると「sleep して待つ」代わりに「割り込みで起こされる」読み出しができる。
     *
     * @return true: 成功
     */
    [[nodiscard]] bool enable_conversion_ready_pin() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace embedded
