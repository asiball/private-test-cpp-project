#include "sensor.hpp"
#include "../../tests/mocks/mock_spi_driver.hpp"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <condition_variable>
#include <mutex>
#include <unistd.h>

using namespace embedded;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArrayArgument;
using ::testing::_;

// MCP3008 の応答をモックする ACTION:
// 引数 rx に [_, msb, lsb] を書き込む（msb の下位2ビット + lsb で 10bit）。
ACTION_P(FillMcp3008Rx, raw10) {
    uint8_t* rx = static_cast<uint8_t*>(arg1);
    rx[0] = 0;
    rx[1] = static_cast<uint8_t>((raw10 >> 8) & 0x03);
    rx[2] = static_cast<uint8_t>(raw10 & 0xFF);
}

// UT-LIB-001: 有効パスで open() 成功（実機のみ）
TEST(SensorOpen, ValidDeviceReturnsTrue) {
    const char* dev = "/dev/spidev0.0";
    if (access(dev, F_OK) != 0) GTEST_SKIP() << dev << " not available";

    Sensor s(dev);
    EXPECT_TRUE(s.open());
    EXPECT_TRUE(s.is_open());
}

//! [UT-LIB-002]
// UT-LIB-002: 無効パスで open() 失敗
TEST(SensorOpen, InvalidDeviceReturnsFalse) {
    Sensor s("/dev/spidevXX.0");
    EXPECT_FALSE(s.open());
    EXPECT_FALSE(s.is_open());
}
//! [UT-LIB-002]

//! [UT-LIB-003]
// UT-LIB-003: read_raw() が MCP3008 プロトコルどおり 10bit 値を返す
TEST(SensorReadRaw, ReturnsTenBitValueViaMock) {
    MockSpiDriver mock;
    EXPECT_CALL(mock, open(_)).WillOnce(Return(true));
    EXPECT_CALL(mock, is_open()).WillRepeatedly(Return(true));
    // 0x2A5 (677) を返すように mock を設定
    EXPECT_CALL(mock, transfer(_, _, 3))
        .WillOnce(DoAll(FillMcp3008Rx(0x2A5), Return(3)));

    Sensor s(&mock);
    ASSERT_TRUE(s.open());
    auto raw = s.read_raw(0);
    ASSERT_TRUE(raw.has_value());
    EXPECT_EQ(*raw, 0x2A5);
}
//! [UT-LIB-003]

//! [UT-LIB-004]
// UT-LIB-004: 範囲外チャネルは std::nullopt
TEST(SensorReadRaw, InvalidChannelReturnsNullopt) {
    MockSpiDriver mock;
    EXPECT_CALL(mock, transfer(_, _, _)).Times(0);  // transfer は呼ばれない

    Sensor s(&mock);
    EXPECT_FALSE(s.read_raw(Sensor::CHANNEL_COUNT).has_value());
    EXPECT_FALSE(s.read_raw(255).has_value());
}
//! [UT-LIB-004]

//! [UT-LIB-005]
// UT-LIB-005: read_voltage() が vref に応じて変換される
TEST(SensorReadVoltage, ScalesByVref) {
    MockSpiDriver mock;
    EXPECT_CALL(mock, is_open()).WillRepeatedly(Return(true));
    // Vref=5.0V のとき、raw=512 ≒ 中央値 → 約 2.5V
    EXPECT_CALL(mock, transfer(_, _, 3))
        .WillOnce(DoAll(FillMcp3008Rx(512), Return(3)));

    Sensor s(&mock, /*vref=*/5.0);
    auto v = s.read_voltage(0);
    ASSERT_TRUE(v.has_value());
    EXPECT_NEAR(*v, 512.0 * 5.0 / Sensor::ADC_MAX, 1e-9);
}
//! [UT-LIB-005]

// UT-LIB-006: set_vref で vref が変更され、以降の read_voltage に反映される
TEST(SensorReadVoltage, SetVrefAffectsConversion) {
    MockSpiDriver mock;
    EXPECT_CALL(mock, is_open()).WillRepeatedly(Return(true));
    EXPECT_CALL(mock, transfer(_, _, 3))
        .WillRepeatedly(DoAll(FillMcp3008Rx(Sensor::ADC_MAX), Return(3)));

    Sensor s(&mock);                       // vref=3.3 (default)
    auto v33 = s.read_voltage(0);
    ASSERT_TRUE(v33.has_value());
    EXPECT_NEAR(*v33, 3.3, 1e-9);

    s.set_vref(5.0);
    auto v50 = s.read_voltage(0);
    ASSERT_TRUE(v50.has_value());
    EXPECT_NEAR(*v50, 5.0, 1e-9);
}

//! [UT-LIB-007]
// UT-LIB-007: read_raw_async() のコールバックが呼ばれる（未オープン時）
TEST(SensorReadRawAsync, NotOpenCallbackReceivesError) {
    Sensor s("/dev/spidevXX.0");   // 存在しないデバイス

    std::mutex mtx;
    std::condition_variable cv;
    bool                    done = false;
    std::optional<uint16_t> received_raw;
    int                     received_err = 0;

    // 共有データ (received_raw / received_err / done) は mtx で保護する。
    // detach されたワーカースレッドのライフタイム vs テスト関数スコープの
    // 競合 (cv の破棄 vs notify_one) を避けるため、notify_one もロック内で呼ぶ。
    s.read_raw_async(0, [&](std::optional<uint16_t> raw, int err) {
        std::lock_guard<std::mutex> lk(mtx);
        received_raw = raw;
        received_err = err;
        done         = true;
        cv.notify_one();
    });

    std::unique_lock<std::mutex> lk(mtx);
    cv.wait_for(lk, std::chrono::seconds(3), [&]{ return done; });
    EXPECT_TRUE(done);
    EXPECT_FALSE(received_raw.has_value());
    EXPECT_NE(received_err, 0);
}
//! [UT-LIB-007]

// UT-LIB-008: MCP3008 の TX 列がプロトコル仕様どおり
TEST(SensorReadRaw, SendsCorrectMcp3008Command) {
    MockSpiDriver mock;
    EXPECT_CALL(mock, is_open()).WillRepeatedly(Return(true));

    uint8_t captured[3] = {0xFF, 0xFF, 0xFF};
    EXPECT_CALL(mock, transfer(_, _, 3))
        .WillOnce([&](const uint8_t* tx, uint8_t*, size_t) {
            captured[0] = tx[0];
            captured[1] = tx[1];
            captured[2] = tx[2];
            return 3;
        });

    Sensor s(&mock);
    (void)s.read_raw(/*channel=*/3);   // CH3 を読む
    EXPECT_EQ(captured[0], 0x01);
    EXPECT_EQ(captured[1], 0x80 | (3 << 4));   // 0xB0
    EXPECT_EQ(captured[2], 0x00);
}

// UT-LIB-009: コピー禁止確認
TEST(SensorCopyable, IsNotCopyConstructible) {
    EXPECT_FALSE(std::is_copy_constructible<Sensor>::value);
    EXPECT_FALSE(std::is_copy_assignable<Sensor>::value);
}
