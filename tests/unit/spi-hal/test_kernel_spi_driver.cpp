#include "kernel_spi_driver.hpp"
#include <gtest/gtest.h>
#include <cerrno>
#include <cstdint>
#include <unistd.h>

using namespace embedded;

// UT-KDRV-001: 有効パス（/dev/my_spi_dev）でopen()成功
TEST(KernelSpiDriverOpen, ValidDeviceReturnsTrue) {
    // my_spi_driver.ko がロードされた実機環境のみパス。
    // CI環境ではスキップ（GTEST_SKIP）。
    const char* dev = "/dev/my_spi_dev";
    if (access(dev, F_OK) != 0) GTEST_SKIP() << dev << " not available";

    KernelSpiDriver drv(dev);
    KernelSpiDriver::Config cfg{1000000, 8, 0};
    EXPECT_TRUE(drv.open(cfg));
    EXPECT_TRUE(drv.is_open());
}

//! [UT-KDRV-002]
// UT-KDRV-002: 存在しないパスでopen()失敗
TEST(KernelSpiDriverOpen, InvalidDeviceReturnsFalse) {
    KernelSpiDriver drv("/dev/no_such_device");
    KernelSpiDriver::Config cfg{1000000, 8, 0};
    EXPECT_FALSE(drv.open(cfg));
    EXPECT_FALSE(drv.is_open());
    EXPECT_NE(drv.last_errno(), 0);
}
//! [UT-KDRV-002]

// UT-KDRV-003: open後にis_open()==true
TEST(KernelSpiDriverOpen, AfterOpenIsOpenTrue) {
    const char* dev = "/dev/my_spi_dev";
    if (access(dev, F_OK) != 0) GTEST_SKIP() << dev << " not available";

    KernelSpiDriver drv(dev);
    KernelSpiDriver::Config cfg{1000000, 8, 0};
    ASSERT_TRUE(drv.open(cfg));
    EXPECT_TRUE(drv.is_open());
}

// UT-KDRV-004: close()後にis_open()==false
TEST(KernelSpiDriverClose, AfterCloseIsOpenFalse) {
    const char* dev = "/dev/my_spi_dev";
    if (access(dev, F_OK) != 0) GTEST_SKIP() << dev << " not available";

    KernelSpiDriver drv(dev);
    KernelSpiDriver::Config cfg{1000000, 8, 0};
    ASSERT_TRUE(drv.open(cfg));
    drv.close();
    EXPECT_FALSE(drv.is_open());
}

//! [UT-KDRV-005]
// UT-KDRV-005: 未openのままclose()しても安全
TEST(KernelSpiDriverClose, DoubleCloseIsSafe) {
    KernelSpiDriver drv("/dev/no_such_device");
    EXPECT_NO_FATAL_FAILURE({
        drv.close();
        drv.close();
    });
}
//! [UT-KDRV-005]

//! [UT-KDRV-006]
// UT-KDRV-006: 未open状態でtransfer()は-1を返す
TEST(KernelSpiDriverTransfer, NotOpenReturnsMinusOne) {
    KernelSpiDriver drv("/dev/my_spi_dev");
    uint8_t tx[4] = {}, rx[4] = {};
    EXPECT_EQ(drv.transfer(tx, rx, 4), -1);
    EXPECT_EQ(drv.last_errno(), EBADF);
}
//! [UT-KDRV-006]

// UT-KDRV-007: コピーが禁止されていることをコンパイル時に確認
TEST(KernelSpiDriverCopyable, IsNotCopyConstructible) {
    EXPECT_FALSE(std::is_copy_constructible<KernelSpiDriver>::value);
    EXPECT_FALSE(std::is_copy_assignable<KernelSpiDriver>::value);
}

// UT-KDRV-008: nullptr tx でtransfer()は-1を返す
TEST(KernelSpiDriverTransfer, NullTxReturnsMinusOne) {
    const char* dev = "/dev/my_spi_dev";
    if (access(dev, F_OK) != 0) GTEST_SKIP() << dev << " not available";

    KernelSpiDriver drv(dev);
    KernelSpiDriver::Config cfg{1000000, 8, 0};
    ASSERT_TRUE(drv.open(cfg));

    uint8_t rx[4] = {};
    EXPECT_EQ(drv.transfer(nullptr, rx, 4), -1);
}

// UT-KDRV-009: len==0 のtransfer()は0を返す（no-op）
TEST(KernelSpiDriverTransfer, ZeroLenReturnsZero) {
    const char* dev = "/dev/my_spi_dev";
    if (access(dev, F_OK) != 0) GTEST_SKIP() << dev << " not available";

    KernelSpiDriver drv(dev);
    KernelSpiDriver::Config cfg{1000000, 8, 0};
    ASSERT_TRUE(drv.open(cfg));

    uint8_t tx[4] = {}, rx[4] = {};
    EXPECT_EQ(drv.transfer(tx, rx, 0), 0);
}

// UT-KDRV-011: len > UINT32_MAX の transfer() は -1 (EOVERFLOW) を返す
TEST(KernelSpiDriverTransfer, OverflowLenReturnsMinusOne) {
    const char* dev = "/dev/my_spi_dev";
    if (access(dev, F_OK) != 0) GTEST_SKIP() << dev << " not available";

    KernelSpiDriver drv(dev);
    KernelSpiDriver::Config cfg{1000000, 8, 0};
    ASSERT_TRUE(drv.open(cfg));

    uint8_t tx[1] = {}, rx[1] = {};
    size_t huge = static_cast<size_t>(UINT32_MAX) + 1;  // 64bit 環境前提
    EXPECT_EQ(drv.transfer(tx, rx, huge), -1);
    EXPECT_EQ(drv.last_errno(), EOVERFLOW);
}

// UT-KDRV-010: close()なしで2回open()するとエラーを返す（既存fdはリークしない）
TEST(KernelSpiDriverOpen, DoubleOpenWithoutCloseReturnsFalse) {
    KernelSpiDriver drv("/dev/no_such_device");
    KernelSpiDriver::Config cfg{1000000, 8, 0};
    // 1回目は失敗（デバイスが存在しない）→ fd_ は -1 のまま
    EXPECT_FALSE(drv.open(cfg));
    EXPECT_FALSE(drv.is_open());

    // 実機向け: fd_ >= 0 の状態で2回目を呼ぶ
    const char* dev = "/dev/my_spi_dev";
    if (access(dev, F_OK) != 0) GTEST_SKIP() << dev << " not available";

    KernelSpiDriver drv2(dev);
    ASSERT_TRUE(drv2.open(cfg));
    EXPECT_TRUE(drv2.is_open());
    // 2回目の open() → false を返し fd_ は変化しない
    EXPECT_FALSE(drv2.open(cfg));
    EXPECT_TRUE(drv2.is_open());  // 既存のfdは保持されたまま
}
