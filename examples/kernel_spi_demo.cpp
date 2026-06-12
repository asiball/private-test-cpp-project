// ─────────────────────────────────────────────────────────────
// KernelSpiDriver 最小サンプル（自作カーネルドライバ経由の SPI 転送）
//
// open() → transfer() → close() の最小フロー。Linux 標準の spidev ではなく、
// 本プロジェクトの自作カーネルモジュール my_spi_driver.ko が提供する
// /dev/my_spi_dev を使う。
//
// 前提: 自作カーネルモジュールをロード済みであること。
//         sudo insmod kernel/my_spi_driver.ko    （または DT オーバーレイで probe）
//       ロードされていない場合 open() は失敗する（/dev/my_spi_dev が無い）。
//
// spi-hal が揃う時のみビルドされる。
// ─────────────────────────────────────────────────────────────
#include "kernel_spi_driver.hpp"

#include <cstdint>
#include <cstdio>

int main()
{
    embedded::KernelSpiDriver drv("/dev/my_spi_dev");

    embedded::KernelSpiDriver::Config cfg{1000000, 8, 0};   // 1MHz, 8bit, MODE0
    if (!drv.open(cfg)) {
        std::fprintf(stderr,
                     "open failed: errno=%d "
                     "(my_spi_driver.ko はロード済みですか？)\n",
                     drv.last_errno());
        return 1;
    }

    // MCP3008: スタートビット, シングルエンド CH0 選択, ダミー
    uint8_t tx[3] = {0x01, 0x80, 0x00};
    uint8_t rx[3] = {0, 0, 0};

    int n = drv.transfer(tx, rx, sizeof(tx));
    if (n < 0) {
        std::fprintf(stderr, "transfer failed: errno=%d\n", drv.last_errno());
        return 1;
    }

    uint16_t raw = static_cast<uint16_t>((rx[1] & 0x03) << 8) | rx[2];
    std::printf("transferred %d bytes, CH0 raw = %u\n", n, raw);

    drv.close();   // デストラクタでも close されるが明示する
    return 0;
}
