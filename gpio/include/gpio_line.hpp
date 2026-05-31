#pragma once
#include <string>

namespace embedded {

/**
 * @brief GPIO 1 ラインのエッジ割り込み（イベント）を扱うクラス
 *
 * Linux の GPIO キャラクタデバイス（/dev/gpiochipN）の v2 uABI を直接 ioctl で叩く。
 * 外部ライブラリ（libgpiod）に依存しないため、このコンポーネント単体でビルドできる。
 *
 * @par なぜ「割り込み」なのか — ポーリングとの対比
 * SPI/I2C のセンサー読み出しは「こちらから叩いて結果を取る（ポーリング）」モデル。
 * 一方 GPIO エッジ検出は「相手が変化したら通知が来る（イベント駆動）」モデルで、
 * CPU を無駄に回さずに待てる。本クラスは epoll で「イベントが来るまで眠る」を実現する。
 * ADS1115 の ALERT/RDY ピンと組み合わせると、「sleep して待つ」代わりに
 * 「変換完了の割り込みで起こされてから読む」構成が作れる。
 *
 * @par C 経験者向けのメモ
 * fd を握って ioctl/read する流れは C とまったく同じ。違いはデストラクタで fd を
 * 自動 close する点（RAII）。C なら goto err; で close を一箇所に集める所を、
 * C++ では「スコープを抜けたら必ず閉じる」ことを型が保証する。
 *
 * @note スレッドセーフではない。コピー禁止。
 */
class GpioLine {
public:
    /** @brief 検出するエッジ */
    enum class Edge { Rising, Falling, Both };

    /**
     * @brief コンストラクタ
     * @param chip_path GPIO チップのパス（例: "/dev/gpiochip0"）
     * @param offset    チップ内のラインオフセット（GPIO 番号）
     */
    GpioLine(const std::string& chip_path, unsigned int offset);

    /** @brief デストラクタ。確保した fd を自動的に close する */
    ~GpioLine();

    GpioLine(const GpioLine&)            = delete;
    GpioLine& operator=(const GpioLine&) = delete;

    /**
     * @brief ラインを入力 + エッジ検出として要求する
     * @param edge 検出するエッジ
     * @return true: 成功
     */
    [[nodiscard]] bool request_edge_events(Edge edge) noexcept;

    /**
     * @brief エッジイベントを epoll で待つ
     * @param timeout_ms タイムアウト [ms]（負値で無限待ち）
     * @return 1: イベント発生（読み出し済み） / 0: タイムアウト / -1: エラー
     */
    [[nodiscard]] int wait_event(int timeout_ms) noexcept;

    /**
     * @brief イベント fd を返す（外部の epoll ループに組み込む用途）
     * @return ライン fd。未要求なら -1
     */
    [[nodiscard]] int event_fd() const noexcept { return line_fd_; }

    /** @brief 確保した fd を閉じる。未確保時は no-op */
    void close() noexcept;

    /** @return エッジ検出を要求済みなら true */
    [[nodiscard]] bool is_requested() const noexcept { return line_fd_ >= 0; }

    /** @return 直近のエラーの errno 値 */
    [[nodiscard]] int last_errno() const noexcept { return last_errno_; }

private:
    std::string  chip_path_;
    unsigned int offset_;
    int          chip_fd_;
    int          line_fd_;
    int          last_errno_;
};

} // namespace embedded
