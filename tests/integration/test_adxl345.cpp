// 結合テスト: ADXL345（SPI 加速度センサ）実機必須
// IT-006 〜 IT-007 に対応
//
// 事前条件:
//   - ADXL345 を SPI0 (/dev/spidev0.0) に接続（CPOL=1/CPHA=1, MODE3）
//   - 実機が無い CI 環境では SetUp で GTEST_SKIP する（MCP3008 結合テストと同方針）
#include "adxl345.hpp"
#include <gtest/gtest.h>
#include <cmath>
#include <unistd.h>

namespace {
constexpr const char* SPI_DEV = "/dev/spidev0.0";
}

class Adxl345IntegrationTest : public ::testing::Test {
protected:
    embedded::Adxl345 dev{SPI_DEV};

    void SetUp() override {
        if (access(SPI_DEV, F_OK) != 0)
            GTEST_SKIP() << "SPI device not available (not a target board)";
        if (!dev.open())
            GTEST_SKIP() << "ADXL345 open failed (未接続の可能性)";
    }
};

// IT-006: DEVID が固定値 0xE5 を返す（疎通・誤配線検出）
TEST_F(Adxl345IntegrationTest, DeviceIdIsExpected) {
    auto id = dev.read_device_id();
    ASSERT_TRUE(id.has_value());
    EXPECT_EQ(*id, 0xE5);   // ADXL345 の DEVID 固定値
}

// IT-007: read_g() が ±16g レンジ内の値を返す
TEST_F(Adxl345IntegrationTest, ReadGWithinRange) {
    auto a = dev.read_g();
    ASSERT_TRUE(a.has_value());
    // ±16g レンジ。静止していても重力 1g 程度は載るため広めに確認する
    EXPECT_LE(std::fabs(a->x), 16.5);
    EXPECT_LE(std::fabs(a->y), 16.5);
    EXPECT_LE(std::fabs(a->z), 16.5);
}
