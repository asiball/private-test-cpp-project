#include "i2c_driver.hpp"
#include <gtest/gtest.h>
#include <cerrno>
#include <cstdint>
#include <unistd.h>

using namespace embedded;

// UT-I2C-001: 存在しないパスで open() 失敗
TEST(I2cDriverOpen, InvalidDeviceReturnsFalse) {
    I2cDriver bus("/dev/i2c-no-such");
    EXPECT_FALSE(bus.open(0x48));
    EXPECT_FALSE(bus.is_open());
    EXPECT_NE(bus.last_errno(), 0);
}

// UT-I2C-002: 未open状態の write() は -1 (EBADF)
TEST(I2cDriverWrite, NotOpenReturnsMinusOne) {
    I2cDriver bus("/dev/i2c-1");
    uint8_t tx[2] = {0x01, 0x02};
    EXPECT_EQ(bus.write(tx, 2), -1);
    EXPECT_EQ(bus.last_errno(), EBADF);
}

// UT-I2C-003: 未open状態の read() は -1 (EBADF)
TEST(I2cDriverRead, NotOpenReturnsMinusOne) {
    I2cDriver bus("/dev/i2c-1");
    uint8_t rx[2] = {};
    EXPECT_EQ(bus.read(rx, 2), -1);
    EXPECT_EQ(bus.last_errno(), EBADF);
}

// UT-I2C-004: 未open状態の write_read() は -1 (EBADF)
TEST(I2cDriverWriteRead, NotOpenReturnsMinusOne) {
    I2cDriver bus("/dev/i2c-1");
    uint8_t tx[1] = {0x00}, rx[2] = {};
    EXPECT_EQ(bus.write_read(tx, 1, rx, 2), -1);
    EXPECT_EQ(bus.last_errno(), EBADF);
}

// UT-I2C-005: 二重 close は安全
TEST(I2cDriverClose, DoubleCloseIsSafe) {
    I2cDriver bus("/dev/i2c-no-such");
    EXPECT_NO_FATAL_FAILURE({
        bus.close();
        bus.close();
    });
}

// UT-I2C-006: コピー禁止確認
TEST(I2cDriverCopyable, IsNotCopyConstructible) {
    EXPECT_FALSE(std::is_copy_constructible<I2cDriver>::value);
    EXPECT_FALSE(std::is_copy_assignable<I2cDriver>::value);
}

// UT-I2C-007: 有効なバスで open()（実機のみ）
TEST(I2cDriverOpen, ValidBusReturnsTrue) {
    const char* dev = "/dev/i2c-1";
    if (access(dev, F_OK) != 0) GTEST_SKIP() << dev << " not available";

    I2cDriver bus(dev);
    EXPECT_TRUE(bus.open(0x48));
    EXPECT_TRUE(bus.is_open());
}
