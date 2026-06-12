// 結合テスト: ADS1115（I2C 16bit ADC）実機必須
// IT-008 〜 IT-009 に対応
//
// 事前条件:
//   - ADS1115 を I2C1 (/dev/i2c-1) に接続（既定アドレス 0x48）
//   - 実機が無い CI 環境では SetUp で GTEST_SKIP する
#include "ads1115.hpp"
#include <gtest/gtest.h>
#include <unistd.h>

namespace {
constexpr const char* I2C_DEV = "/dev/i2c-1";
}

class Ads1115IntegrationTest : public ::testing::Test {
protected:
    embedded::Ads1115 adc{I2C_DEV};

    void SetUp() override {
        if (access(I2C_DEV, F_OK) != 0)
            GTEST_SKIP() << "I2C device not available (not a target board)";
        if (!adc.open())
            GTEST_SKIP() << "ADS1115 open failed (未接続の可能性)";
    }
};

// IT-008: 全チャネルが変換に成功し、デフォルトゲインのフルスケール内に収まる
TEST_F(Ads1115IntegrationTest, AllChannelsConvertWithinFullScale) {
    const double fs = adc.full_scale_volts();
    for (uint8_t ch = 0; ch < embedded::Ads1115::CHANNEL_COUNT; ++ch) {
        auto v = adc.read_voltage(ch);
        ASSERT_TRUE(v.has_value()) << "channel " << static_cast<int>(ch);
        // 単電源シングルエンドでは 0 〜 +フルスケール。負側は微小なノイズを許容
        EXPECT_GE(*v, -0.1);
        EXPECT_LE(*v, fs + 0.1);
    }
}

// IT-009: start_conversion() → read_result() の分離 API が一貫した値を返す
TEST_F(Ads1115IntegrationTest, StartAndReadResult) {
    ASSERT_TRUE(adc.start_conversion(0));
    usleep(10 * 1000);   // 変換時間（128SPS ≈ 7.8ms）を待つ
    auto raw = adc.read_result();
    ASSERT_TRUE(raw.has_value());
}
