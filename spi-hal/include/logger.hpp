#ifndef EDS_COMMON_LOGGER_HPP
#define EDS_COMMON_LOGGER_HPP
#include <cstdio>
#include <cstring>
#include <syslog.h>

// NOTE: これは common/include/logger.hpp と同一 API のログマクロ定義です。
//       共有の正本は common/include/logger.hpp（API-COM-001 / Doxygen 化済み）。
//       spi-hal は「単体で取り出してもビルドできる独立コンポーネント」という方針のため、
//       common/ に依存せず済むよう、あえて同じ内容のコピーを自前で持っています
//       （docs/deliverables/README.md §2 の注記参照）。
//       挙動を変更する場合は両ファイルを同時に更新してください。
//       common/include/logger.hpp と同期を保つこと。ガードマクロ共有により
//       二重 include 時も安全（先に include された方の内容だけが有効になる）。

// ─────────────────────────────────────────────────────────────
// ログマクロ
//
// 注意: syslog.h は LOG_INFO=6, LOG_ERR=3, LOG_DEBUG=7 という
//       整数定数マクロを定義している。そのまま同名のマクロを定義すると
//       プリプロセッサが再帰展開しようとして未定義シンボルエラーになる。
//       そのため LOGI / LOGW / LOGE / LOGD という名前を使用する。
//
// DEBUGビルド (-DDEBUG):
//   stderr に詳細ログ + syslog に記録。
//   端末でリアルタイムに見ながら journalctl にも残せる。
//
// RELEASEビルド (デフォルト):
//   syslog のみ。
//
// 使い方:
//   LOG_OPEN("device-ctl");  // main() の先頭で1回呼ぶ
//   LOGI("opened %s", path);
//   LOGW("retry %d/3", n);
//   LOGE("transfer failed: %d", errno_val);
//   LOGD("tx[0]=0x%02x", tx[0]);  // DEBUGビルドのみ
//   LOG_CLOSE();                   // main() の末尾で呼ぶ
// ─────────────────────────────────────────────────────────────

#define LOG_OPEN(ident)  openlog((ident), LOG_PID | LOG_CONS, LOG_DAEMON)
#define LOG_CLOSE()      closelog()

#ifdef DEBUG
// ── DEBUGビルド: stderr + syslog 両方に出す ──────────────────
#define LOGI(fmt, ...) \
    do { \
        fprintf(stderr, "[INFO]  " fmt "\n", ##__VA_ARGS__); \
        syslog(LOG_INFO,    fmt, ##__VA_ARGS__); \
    } while (0)

#define LOGW(fmt, ...) \
    do { \
        fprintf(stderr, "[WARN]  " fmt "\n", ##__VA_ARGS__); \
        syslog(LOG_WARNING, fmt, ##__VA_ARGS__); \
    } while (0)

#define LOGE(fmt, ...) \
    do { \
        fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__); \
        syslog(LOG_ERR,     fmt, ##__VA_ARGS__); \
    } while (0)

// DEBUGマクロ: stderrのみ（syslogをデバッグログで埋めない）
#define LOGD(fmt, ...) \
    fprintf(stderr, "[DEBUG] " fmt "\n", ##__VA_ARGS__)

#else
// ── RELEASEビルド: syslog のみ ───────────────────────────────
#define LOGI(fmt, ...)  syslog(LOG_INFO,    fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...)  syslog(LOG_WARNING, fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...)  syslog(LOG_ERR,     fmt, ##__VA_ARGS__)
#define LOGD(fmt, ...)  // RELEASE では no-op

#endif // DEBUG

// スレッドセーフな strerror（非スレッドセーフな strerror() の代替）。
// 詳細は common/include/logger.hpp の同名関数を参照。GNU 版（char* を返す）と
// XSI/POSIX 版（int を返す）の両方に戻り値型オーバーロードで対応する。
namespace logging_detail {
inline const char* strerror_r_result(int ret, char* buf) {
    return ret == 0 ? buf : "unknown error";   // POSIX 版: 0 成功
}
inline const char* strerror_r_result(const char* msg, char* /*buf*/) {
    return msg;                                 // GNU 版: メッセージを直接返す
}
} // namespace logging_detail

inline const char* errno_str(int err) {
    static thread_local char buf[128];
    return logging_detail::strerror_r_result(strerror_r(err, buf, sizeof(buf)), buf);
}

#endif // EDS_COMMON_LOGGER_HPP
