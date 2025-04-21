#include "spi_driver.hpp"
#include <gtest/gtest.h>

using namespace embedded;

// UT-DRV-001: 有効パスでopen()成功
TEST(SpiDriverOpen, ValidDeviceReturnsTrue) {
    // /dev/spidev0.0 が存在する実機環境のみパス。
    // CI環境ではスキップ（GTEST_SKIP）。
    const char* dev = "/dev/spidev0.0";
    if (access(dev, F_OK) != 0) GTEST_SKIP() << dev << " not available";

    SpiDriver drv(dev);
    SpiDriver::Config cfg{1000000, 8, 0};
    EXPECT_TRUE(drv.open(cfg));
    EXPECT_TRUE(drv.is_open());
}

// UT-DRV-002: 存在しないパスでopen()失敗
TEST(SpiDriverOpen, InvalidDeviceReturnsFalse) {
    SpiDriver drv("/dev/spidevXX.0");
    SpiDriver::Config cfg{1000000, 8, 0};
    EXPECT_FALSE(drv.open(cfg));
    EXPECT_FALSE(drv.is_open());
    EXPECT_NE(drv.last_errno(), 0);
}

// UT-DRV-003: open後にclose()するとis_open()==false
TEST(SpiDriverClose, AfterOpenIsOpenFalse) {
    const char* dev = "/dev/spidev0.0";
    if (access(dev, F_OK) != 0) GTEST_SKIP();

    SpiDriver drv(dev);
    SpiDriver::Config cfg{1000000, 8, 0};
    ASSERT_TRUE(drv.open(cfg));
    drv.close();
    EXPECT_FALSE(drv.is_open());
}

// UT-DRV-004: 未openのままclose()しても安全
TEST(SpiDriverClose, DoubleCloseIsSafe) {
    SpiDriver drv("/dev/spidevXX.0");
    EXPECT_NO_FATAL_FAILURE({
        drv.close();
        drv.close();
    });
}

// UT-DRV-006: 未open状態でtransfer()は-1を返す
TEST(SpiDriverTransfer, NotOpenReturnsMinusOne) {
    SpiDriver drv("/dev/spidev0.0");
    uint8_t tx[4] = {}, rx[4] = {};
    EXPECT_EQ(drv.transfer(tx, rx, 4), -1);
}

// UT-DRV-008: コピーが禁止されていることをコンパイル時に確認
TEST(SpiDriverCopyable, IsNotCopyConstructible) {
    EXPECT_FALSE(std::is_copy_constructible<SpiDriver>::value);
    EXPECT_FALSE(std::is_copy_assignable<SpiDriver>::value);
}
