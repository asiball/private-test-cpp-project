#include "sensor.hpp"
#include <gtest/gtest.h>
#include <cstdlib>

// 結合テスト: 実機（Raspi3B+ + SPI loopback）必須
// IT-001 〜 IT-006 に対応

static const char* SPI_DEV = "/dev/spidev0.0";

class SpiLoopbackTest : public ::testing::Test {
protected:
    embedded::Sensor dev{SPI_DEV};

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

// IT-004: read_async() のコールバックが呼ばれること（loopback環境）
TEST_F(SpiLoopbackTest, AsyncReadCallbackIsCalled) {
    std::mutex mtx;
    std::condition_variable cv;
    bool   done    = false;
    int    cb_err  = -1;
    size_t cb_size = 0;

    dev.read_async(0x00, 4, [&](const std::vector<uint8_t>& data, int err) {
        cb_err  = err;
        cb_size = data.size();
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
    EXPECT_EQ(cb_size, 4u);
}

// IT-005: read_async() — エラー発生時にコールバックへエラーが通知されること
TEST(SpiDeviceAsync, AsyncReadOnClosedDeviceReceivesError) {
    embedded::Sensor dev("/dev/spidev0.0");   // open しない

    std::mutex mtx;
    std::condition_variable cv;
    bool   done          = false;
    int    received_err  = 0;
    size_t received_size = 99;   // 初期値を非ゼロにして「何も書かれなかった」を検出

    dev.read_async(0x00, 4, [&](const std::vector<uint8_t>& data, int err) {
        received_err  = err;
        received_size = data.size();
        {
            std::lock_guard<std::mutex> lk(mtx);
            done = true;
        }
        cv.notify_one();
    });

    std::unique_lock<std::mutex> lk(mtx);
    cv.wait_for(lk, std::chrono::seconds(3), [&]{ return done; });
    EXPECT_TRUE(done) << "callback was not called within 3 seconds";
    // 未openなので「データが空」または「errが非ゼロ」のどちらかが成立する
    EXPECT_TRUE(received_size == 0 || received_err != 0)
        << "expected empty data or non-zero error, got size=" << received_size
        << " err=" << received_err;
}

// IT-006: 無効デバイスでエラー処理
TEST(SpiDeviceError, InvalidDeviceReturnsError) {
    embedded::Sensor dev("/dev/spidevXX.0");
    EXPECT_FALSE(dev.open());
    EXPECT_TRUE(dev.read(0x00, 1).empty());
}
