#include "sensor.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <unistd.h>

// 結合テスト: 実機（RPi 3B+ + MCP3008 SPI ADC）必須
// IT-001 〜 IT-005 に対応
//
// 事前条件:
//   - MCP3008 を SPI0 に接続
//     - CS  → GPIO8 (CE0)
//     - SCK → GPIO11
//     - DIN → GPIO10 (MOSI)
//     - DOUT→ GPIO9  (MISO)
//   - Vref/Vdd → 3.3V
//   - /dev/spidev0.0 が存在し、ユーザに読み書き権限があること

static const char* SPI_DEV = "/dev/spidev0.0";
static constexpr double DEFAULT_VREF = 3.3;

class Mcp3008Test : public ::testing::Test {
protected:
    embedded::Sensor sensor{SPI_DEV, DEFAULT_VREF};

    void SetUp() override {
        if (access(SPI_DEV, F_OK) != 0)
            GTEST_SKIP() << "SPI device not available (not a target board)";
        ASSERT_TRUE(sensor.open()) << "Cannot open " << SPI_DEV;
    }
    void TearDown() override { sensor.close(); }
};

// IT-001: 全チャネルの読み出しが成功し、10bit 範囲に収まる
TEST_F(Mcp3008Test, AllChannelsReturnValidRange) {
    for (uint8_t ch = 0; ch < embedded::Sensor::CHANNEL_COUNT; ++ch) {
        auto raw = sensor.read_raw(ch);
        ASSERT_TRUE(raw.has_value()) << "CH" << static_cast<int>(ch) << " read failed";
        EXPECT_LE(*raw, embedded::Sensor::ADC_MAX)
            << "CH" << static_cast<int>(ch) << " raw=" << *raw;
    }
}

// IT-002: read_voltage() が 0V 〜 Vref の範囲を返す
TEST_F(Mcp3008Test, ReadVoltageWithinVref) {
    for (uint8_t ch = 0; ch < embedded::Sensor::CHANNEL_COUNT; ++ch) {
        auto v = sensor.read_voltage(ch);
        ASSERT_TRUE(v.has_value());
        EXPECT_GE(*v, 0.0);
        EXPECT_LE(*v, DEFAULT_VREF + 1e-6);
    }
}

// IT-003: 100 回連続読み出しでエラー 0 件
TEST_F(Mcp3008Test, Stability100ReadsOnCh0) {
    int errors = 0;
    for (int i = 0; i < 100; ++i) {
        if (!sensor.read_raw(0).has_value()) ++errors;
    }
    EXPECT_EQ(errors, 0) << errors << " errors out of 100 reads";
}

// IT-004: read_raw_async() のコールバックが呼ばれ、有効な値を返す
TEST_F(Mcp3008Test, AsyncReadCallbackIsCalled) {
    std::mutex              mtx;
    std::condition_variable cv;
    bool                    done = false;
    std::optional<uint16_t> result;
    int                     cb_err = -1;

    sensor.read_raw_async(0, [&](std::optional<uint16_t> raw, int err) {
        result = raw;
        cb_err = err;
        {
            std::lock_guard<std::mutex> lk(mtx);
            done = true;
        }
        cv.notify_one();
    });

    std::unique_lock<std::mutex> lk(mtx);
    cv.wait_for(lk, std::chrono::seconds(3), [&]{ return done; });
    EXPECT_TRUE(done) << "callback was not called within 3 seconds";
    EXPECT_EQ(cb_err, 0);
    ASSERT_TRUE(result.has_value());
    EXPECT_LE(*result, embedded::Sensor::ADC_MAX);
}

// IT-005: 無効デバイスでエラー処理
TEST(Mcp3008Error, InvalidDeviceFailsToOpen) {
    embedded::Sensor s("/dev/spidevXX.0");
    EXPECT_FALSE(s.open());
    EXPECT_FALSE(s.read_raw(0).has_value());
}
