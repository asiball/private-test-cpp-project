#pragma once
#include <cerrno>
#include <cstddef>
#include <cstdint>

namespace embedded {
namespace detail {

/**
 * @brief SPI transfer() の引数を検証する（SpiDriver / KernelSpiDriver 共通）。
 *
 * 両ドライバの transfer() で検証順序・内容が同一だったため、単一の情報源に
 * まとめる（過去に片方だけ overflow ガードが欠落していた不整合の温床を解消）。
 *
 * @return 0          検証 OK（転送を実行してよい。len==0 を含む）
 * @return -EBADF     fd 未オープン
 * @return -EINVAL    tx / rx が null
 * @return -EOVERFLOW len が uint32_t を超過（ハードウェアの len フィールドに収まらない）
 *
 * @note len==0 自体はエラーではない（呼び出し側がバスアクセスせず 0 を返す）。
 *       本関数は検証のみを行い、ログ出力や last_errno_ の設定は呼び出し側で行う。
 */
inline int validate_spi_transfer(int fd, const void* tx, const void* rx, size_t len) noexcept
{
    if (fd < 0) {
        return -EBADF;
    }
    if (!tx || !rx) {
        return -EINVAL;
    }
    if (len > static_cast<size_t>(UINT32_MAX)) {
        return -EOVERFLOW;
    }
    return 0;
}

} // namespace detail
} // namespace embedded
