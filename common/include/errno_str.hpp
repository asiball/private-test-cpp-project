#pragma once
#include <cerrno>
#include <cstring>

/**
 * @file errno_str.hpp
 * @brief スレッドセーフな errno 文字列変換ヘルパ
 *
 * 標準の strerror() は内部静的バッファを使うためスレッドセーフでない。
 * このヘルパは thread_local バッファ + strerror_r() (POSIX 版) を使い、
 * CLI のモニタスレッドとメインスレッドが同時に呼んでも安全に動作する。
 *
 * 使い方:
 * @code
 *   #include "errno_str.hpp"
 *   LOGE("open failed: %s", errno_str(errno));
 * @endcode
 */

/**
 * @brief errno 値をスレッドセーフに文字列へ変換する。
 *
 * GNU 版と POSIX 版で strerror_r の戻り値型が異なる (_GNU_SOURCE が
 * 定義されているかで切り替える）。
 *
 * @param e errno 値
 * @return 対応するエラー文字列（スレッドローカル領域。呼び出しごとに上書きされる）
 */
inline const char* errno_str(int e) noexcept {
    static thread_local char buf[128];
#if defined(_GNU_SOURCE)
    // GNU 版: strerror_r は char* を返す
    return strerror_r(e, buf, sizeof(buf));
#else
    // POSIX 版: strerror_r は int を返す; 失敗時は "unknown" を返す
    return (strerror_r(e, buf, sizeof(buf)) == 0) ? buf : "unknown";
#endif
}
