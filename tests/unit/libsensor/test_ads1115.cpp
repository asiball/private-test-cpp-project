#include "ads1115.hpp"
#include "../../tests/mocks/mock_i2c_driver.hpp"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstring>

using namespace embedded;
using ::testing::_;
using ::testing::Return;

namespace {
// write_read のモック共通処理:
//   tx[0]==0x01 (Config)     → OS ビットを立てた値を返す（変換完了とみなす）
//   tx[0]==0x00 (Conversion) → raw16 を MSB first で返す
auto MakeWriteReadFake(uint16_t raw16) {
    return [raw16](const uint8_t* tx, size_t, uint8_t* rx, size_t rx_len) -> int {
        if (rx_len < 2) return -1;
        if (tx[0] == 0x01) {            // Config 読み出し（OS ポーリング）
            rx[0] = 0x80;               // OS=1 → 変換完了
            rx[1] = 0x00;
        } else {                        // Conversion 読み出し
            rx[0] = static_cast<uint8_t>(raw16 >> 8);
            rx[1] = static_cast<uint8_t>(raw16 & 0xFF);
        }
        return static_cast<int>(rx_len);
    };
}
} // namespace

// UT-ADS-001: 無効パスで open() 失敗（実機ドライバ経由）
TEST(Ads1115Open, InvalidDeviceReturnsFalse) {
    Ads1115 adc("/dev/i2cXX");
    EXPECT_FALSE(adc.open());
    EXPECT_FALSE(adc.is_open());
}

// UT-ADS-002: 範囲外チャネルは std::nullopt（バスアクセスは発生しない）
TEST(Ads1115ReadRaw, InvalidChannelReturnsNullopt) {
    MockI2cDriver mock;
    EXPECT_CALL(mock, write(_, _)).Times(0);
    EXPECT_CALL(mock, write_read(_, _, _, _)).Times(0);

    Ads1115 adc(&mock);
    EXPECT_FALSE(adc.read_raw(Ads1115::CHANNEL_COUNT).has_value());
    EXPECT_FALSE(adc.read_raw(255).has_value());
}

// UT-ADS-003: read_raw() が変換レジスタの値を返す
TEST(Ads1115ReadRaw, ReturnsConversionValueViaMock) {
    MockI2cDriver mock;
    EXPECT_CALL(mock, write(_, _)).WillRepeatedly(Return(3));
    EXPECT_CALL(mock, write_read(_, _, _, _))
        .WillRepeatedly(MakeWriteReadFake(0x4000));  // 16384

    Ads1115 adc(&mock);
    auto raw = adc.read_raw(0);
    ASSERT_TRUE(raw.has_value());
    EXPECT_EQ(*raw, 0x4000);
}

// UT-ADS-004: read_voltage() がフルスケールとゲインで変換される
TEST(Ads1115ReadVoltage, ScalesByFullScale) {
    MockI2cDriver mock;
    EXPECT_CALL(mock, write(_, _)).WillRepeatedly(Return(3));
    EXPECT_CALL(mock, write_read(_, _, _, _))
        .WillRepeatedly(MakeWriteReadFake(16384));  // フルスケールの半分

    Ads1115 adc(&mock);                  // デフォルト ±2.048V
    auto v = adc.read_voltage(0);
    ASSERT_TRUE(v.has_value());
    EXPECT_NEAR(*v, 16384.0 * 2.048 / 32768.0, 1e-9);  // ≒ 1.024V
}

// UT-ADS-005: set_gain で full_scale_volts が変わる
TEST(Ads1115Gain, SetGainChangesFullScale) {
    MockI2cDriver mock;
    Ads1115 adc(&mock);
    EXPECT_NEAR(adc.full_scale_volts(), 2.048, 1e-9);  // デフォルト
    adc.set_gain(Ads1115::Gain::FSR_4_096V);
    EXPECT_NEAR(adc.full_scale_volts(), 4.096, 1e-9);
}

// UT-ADS-006: read_raw() の Config が OS + シングルエンド MUX を含む
TEST(Ads1115ReadRaw, SendsCorrectConfig) {
    MockI2cDriver mock;
    uint8_t captured[3] = {0, 0, 0};
    EXPECT_CALL(mock, write(_, _))
        .WillOnce([&](const uint8_t* data, size_t len) -> int {
            if (len >= 3) std::memcpy(captured, data, 3);
            return static_cast<int>(len);
        })
        .WillRepeatedly(Return(3));
    EXPECT_CALL(mock, write_read(_, _, _, _))
        .WillRepeatedly(MakeWriteReadFake(0));

    Ads1115 adc(&mock);
    (void)adc.read_raw(0);   // CH0
    EXPECT_EQ(captured[0], 0x01);   // Config レジスタポインタ
    // OS=1, MUX=A0 single-ended(0x4000), PGA=±2.048(0x0400), MODE=single(0x0100),
    // DR=128SPS(0x0080), COMP_QUE=disable(0x0003) → 0xC583
    EXPECT_EQ(captured[1], 0xC5);
    EXPECT_EQ(captured[2], 0x83);
}

// UT-ADS-007: コピー禁止確認
TEST(Ads1115Copyable, IsNotCopyConstructible) {
    EXPECT_FALSE(std::is_copy_constructible<Ads1115>::value);
    EXPECT_FALSE(std::is_copy_assignable<Ads1115>::value);
}
