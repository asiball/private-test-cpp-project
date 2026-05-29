#pragma once
#include <cstdio>
#include <syslog.h>

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
