#pragma once
#include <cstdio>
#include <syslog.h>

/**
 * @file logger.hpp
 * @brief 共有ログマクロ（LOGI / LOGW / LOGE / LOGD / LOG_OPEN / LOG_CLOSE）
 *
 * i2c-hal / gpio など新規コンポーネント用の共有ログ基盤。
 * spi-hal/include/logger.hpp と同一の API を提供し、spi-hal を取り除いても
 * 新規コンポーネントが単体でビルドできるよう、依存を持たない common/ 配下に置く。
 *
 * @note syslog.h は LOG_INFO=6 / LOG_ERR=3 / LOG_DEBUG=7 を整数マクロとして
 *       定義済み。同名衝突を避けるため LOGI / LOGW / LOGE / LOGD を使う。
 *
 * ビルド別の挙動:
 * - DEBUG ビルド (`-DDEBUG`): stderr + syslog の両方へ出力
 * - RELEASE ビルド（デフォルト）: syslog のみ（LOGD は no-op）
 *
 * 詳細な仕様は docs/deliverables/04_api-spec/common-logger-api.md（API-COM-001）を参照。
 */

/**
 * @def LOG_OPEN
 * @brief syslog をオープンする（main() の先頭で 1 回呼ぶ）。
 * @param ident ログ識別子（openlog が保持するため寿命のある文字列を渡す）
 */
#define LOG_OPEN(ident)  openlog((ident), LOG_PID | LOG_CONS, LOG_DAEMON)

/**
 * @def LOG_CLOSE
 * @brief syslog をクローズする（main() の末尾で呼ぶ）。
 */
#define LOG_CLOSE()      closelog()

#ifdef DEBUG
/** @def LOGI @brief 情報ログ（INFO）。DEBUG ビルドでは stderr + syslog(LOG_INFO)。 */
#define LOGI(fmt, ...) \
    do { \
        fprintf(stderr, "[INFO]  " fmt "\n", ##__VA_ARGS__); \
        syslog(LOG_INFO,    fmt, ##__VA_ARGS__); \
    } while (0)

/** @def LOGW @brief 警告ログ（WARN）。DEBUG ビルドでは stderr + syslog(LOG_WARNING)。 */
#define LOGW(fmt, ...) \
    do { \
        fprintf(stderr, "[WARN]  " fmt "\n", ##__VA_ARGS__); \
        syslog(LOG_WARNING, fmt, ##__VA_ARGS__); \
    } while (0)

/** @def LOGE @brief エラーログ（ERROR）。DEBUG ビルドでは stderr + syslog(LOG_ERR)。 */
#define LOGE(fmt, ...) \
    do { \
        fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__); \
        syslog(LOG_ERR,     fmt, ##__VA_ARGS__); \
    } while (0)

/** @def LOGD @brief デバッグログ（DEBUG）。DEBUG ビルドでは stderr のみ（syslog を埋めない）。 */
#define LOGD(fmt, ...) \
    fprintf(stderr, "[DEBUG] " fmt "\n", ##__VA_ARGS__)

#else
// RELEASE ビルド: syslog のみ（LOGD は no-op）
#define LOGI(fmt, ...)  syslog(LOG_INFO,    fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...)  syslog(LOG_WARNING, fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...)  syslog(LOG_ERR,     fmt, ##__VA_ARGS__)
#define LOGD(fmt, ...)  // RELEASE では no-op
#endif // DEBUG
