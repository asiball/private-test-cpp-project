#pragma once
#include <cstdio>
#include <syslog.h>

// ─────────────────────────────────────────────────────────────
// ログマクロ
//
// DEBUG ビルド (-DDEBUG):
//   stderr に詳細ログを出しつつ syslog にも書く。
//   端末でリアルタイムに見ながら、journalctl にも残せる。
//
// RELEASE ビルド (デフォルト / -DNDEBUG):
//   syslog のみ。stderr には何も出さない。
//   本番デーモンとして動かす想定。
//
// 使い方:
//   LOG_OPEN("spi-driver");          // main() の先頭で1回呼ぶ
//   LOG_INFO("opened %s", path);
//   LOG_WARN("retry %d/3", n);
//   LOG_ERR("transfer failed: %d", errno);
//   LOG_DEBUG("tx[0]=0x%02x", tx[0]); // DEBUGビルドのみ出力
//   LOG_CLOSE();                       // main() の末尾で呼ぶ
// ─────────────────────────────────────────────────────────────

#define LOG_OPEN(ident)  openlog((ident), LOG_PID | LOG_CONS, LOG_DAEMON)
#define LOG_CLOSE()      closelog()

#ifdef DEBUG
// ── DEBUGビルド: stderr + syslog 両方に出す ──────────────────
#define LOG_INFO(fmt, ...) \
    do { \
        fprintf(stderr, "[INFO]  " fmt "\n", ##__VA_ARGS__); \
        syslog(LOG_INFO,    fmt, ##__VA_ARGS__); \
    } while (0)

#define LOG_WARN(fmt, ...) \
    do { \
        fprintf(stderr, "[WARN]  " fmt "\n", ##__VA_ARGS__); \
        syslog(LOG_WARNING, fmt, ##__VA_ARGS__); \
    } while (0)

#define LOG_ERR(fmt, ...) \
    do { \
        fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__); \
        syslog(LOG_ERR,     fmt, ##__VA_ARGS__); \
    } while (0)

// DEBUGマクロ: stderrのみ（syslogをデバッグログで埋めない）
#define LOG_DEBUG(fmt, ...) \
    fprintf(stderr, "[DEBUG] " fmt "\n", ##__VA_ARGS__)

#else
// ── RELEASEビルド: syslog のみ ───────────────────────────────
#define LOG_INFO(fmt, ...)  syslog(LOG_INFO,    fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  syslog(LOG_WARNING, fmt, ##__VA_ARGS__)
#define LOG_ERR(fmt, ...)   syslog(LOG_ERR,     fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) // RELEASE では no-op

#endif // DEBUG
