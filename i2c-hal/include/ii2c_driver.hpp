#pragma once
#include <cstddef>
#include <cstdint>

namespace embedded {

/**
 * @brief I2C ドライバの抽象インターフェース
 *
 * I2cDriver（実機用）と MockI2cDriver（テスト用）が共通の型として扱われる。
 * 役割は spi-hal の ISpiDriver と同じで、上位（ADS1115 など）を実機から切り離す。
 *
 * @par C 経験者向けのメモ
 * このクラスは「純粋仮想関数だけを持つクラス＝インターフェース」である。
 * C で書くなら、関数ポインタを並べた ops 構造体に相当する:
 * @code
 *   // C 版（よくあるドライバの書き方）
 *   struct i2c_ops {
 *       int  (*open)(void* self, uint16_t addr);
 *       void (*close)(void* self);
 *       int  (*write)(void* self, const uint8_t* d, size_t n);
 *       ...
 *   };
 * @endcode
 * C++ では vtable がこの ops 表を自動生成し、`drv->write(...)` が正しい実装へ
 * ディスパッチされる。手書きの関数ポインタ表が言語機能になったもの、と捉えるとよい。
 *
 * @note SPI（フルデュプレクス転送）と違い、I2C は write / read を別操作として扱う。
 *       レジスタ読み出しは「ポインタ書き込み → リピーテッドスタート → 読み出し」が定石で、
 *       これを write_read() が 1 トランザクションで行う。
 */
class II2cDriver {
public:
    virtual ~II2cDriver() = default;

    /**
     * @brief バスをオープンし、通信相手のスレーブアドレスを設定する
     * @param addr 7bit スレーブアドレス（例: ADS1115 = 0x48）
     * @return true: 成功 / false: 失敗
     */
    [[nodiscard]] virtual bool open(uint16_t addr) noexcept = 0;

    /** @brief バスをクローズする。未オープン時は no-op */
    virtual void close() noexcept = 0;

    /**
     * @brief スレーブへ len バイト書き込む
     * @return 書き込んだバイト数。エラー時は -1
     */
    [[nodiscard]] virtual int write(const uint8_t* data, size_t len) noexcept = 0;

    /**
     * @brief スレーブから len バイト読み出す
     * @return 読み出したバイト数。エラー時は -1
     */
    [[nodiscard]] virtual int read(uint8_t* data, size_t len) noexcept = 0;

    /**
     * @brief 書き込み → リピーテッドスタート → 読み出し を 1 トランザクションで行う
     *
     * レジスタ参照の定番（例: ADS1115 の変換結果読み出し）。
     * @param tx     書き込みバイト列（通常はレジスタポインタ）
     * @param tx_len tx の長さ
     * @param rx     読み出しバッファ
     * @param rx_len 読み出す長さ
     * @return 読み出したバイト数。エラー時は -1
     */
    [[nodiscard]] virtual int write_read(const uint8_t* tx, size_t tx_len,
                                         uint8_t* rx, size_t rx_len) noexcept = 0;

    /** @return バスがオープン中なら true */
    [[nodiscard]] virtual bool is_open() const noexcept = 0;

    /** @return 直近のエラーの errno 値 */
    [[nodiscard]] virtual int last_errno() const noexcept = 0;
};

} // namespace embedded
