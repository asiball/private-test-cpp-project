#pragma once
#include <cstdio>
#include <syslog.h>

// ─────────────────────────────────────────────────────────────
// 共有ログマクロ（i2c-hal / gpio など新規コンポーネント用）
//
// spi-hal/include/logger.hpp と同一の API（LOGI/LOGW/LOGE/LOGD/LOG_OPEN/
// LOG_CLOSE）を提供する。spi-hal を取り除いても新規コンポーネントが単体で
// ビルドできるよう、依存を持たない common/ 配下に置いている。
//
// 注意: syslog.h は LOG_INFO=6, LOG_ERR=3, LOG_DEBUG=7 を整数マクロとして
//       定義済み。そのため同名を避け LOGI / LOGW / LOGE / LOGD を使う。
//
// DEBUGビルド (-DDEBUG): stderr + syslog の両方
// RELEASEビルド (デフォルト): syslog のみ
// ─────────────────────────────────────────────────────────────

#define LOG_OPEN(ident)  openlog((ident), LOG_PID | LOG_CONS, LOG_DAEMON)
#define LOG_CLOSE()      closelog()

#ifdef DEBUG
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

#define LOGD(fmt, ...) \
    fprintf(stderr, "[DEBUG] " fmt "\n", ##__VA_ARGS__)

#else
#define LOGI(fmt, ...)  syslog(LOG_INFO,    fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...)  syslog(LOG_WARNING, fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...)  syslog(LOG_ERR,     fmt, ##__VA_ARGS__)
#define LOGD(fmt, ...)  // RELEASE では no-op
#endif // DEBUG
