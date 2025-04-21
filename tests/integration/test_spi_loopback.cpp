#include "device.hpp"
#include <gtest/gtest.h>
#include <cstdlib>

// 結合テスト: 実機（Raspi4B + SPI loopback）必須
// IT-001 〜 IT-006 に対応

static const char* SPI_DEV = "/dev/spidev0.0";

class SpiLoopbackTest : public ::testing::Test {
protected:
    embedded::Device dev{SPI_DEV};

    void SetUp() override {
        if (access(SPI_DEV, F_OK) != 0)
            GTEST_SKIP() << "SPI device not available (not a target board)";
        ASSERT_TRUE(dev.open()) << "Cannot open " << SPI_DEV;
    }
    void TearDown() override { dev.close(); }
};

// IT-001: 書き込み→読み出しデータ一致
TEST_F(SpiLoopbackTest, WriteReadConsistency) {
    std::vector<uint8_t> payload = {0xDE, 0xAD};
    ASSERT_TRUE(dev.write(0x10, payload));

    auto result = dev.read(0x10, 2);
    ASSERT_EQ(result.size(), 2u);
    // loopback: TX と RX が一致する
    EXPECT_EQ(result[0], 0x10 & 0x7F);  // addr byte
    EXPECT_EQ(result[1], 0xDE);
}

// IT-002: 連続転送10回で全てエラーなし
TEST_F(SpiLoopbackTest, ConsecutiveRead10Times) {
    for (int i = 0; i < 10; ++i) {
        auto result = dev.read(0x00, 8);
        EXPECT_EQ(result.size(), 8u) << "failed at iteration " << i;
    }
}

// IT-003: 100回連続読み出し安定性（1MHz）
TEST_F(SpiLoopbackTest, Stability100Reads) {
    int errors = 0;
    for (int i = 0; i < 100; ++i) {
        if (dev.read(0x00, 64).size() != 64u) ++errors;
    }
    EXPECT_EQ(errors, 0) << errors << " errors out of 100 transfers";
}

// IT-006: 無効デバイスでエラー処理
TEST(SpiDeviceError, InvalidDeviceReturnsError) {
    embedded::Device dev("/dev/spidevXX.0");
    EXPECT_FALSE(dev.open());
    EXPECT_TRUE(dev.read(0x00, 1).empty());
}
