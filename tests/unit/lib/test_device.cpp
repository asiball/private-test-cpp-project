#include "device.hpp"
#include "../../tests/mocks/mock_spi_driver.hpp"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <condition_variable>
#include <mutex>
#include <unistd.h>

using namespace embedded;
using ::testing::Return;
using ::testing::_;

// UT-LIB-001: 有効パスでopen()成功（実機のみ）
TEST(DeviceOpen, ValidDeviceReturnsTrue) {
    const char* dev = "/dev/spidev0.0";
    if (access(dev, F_OK) != 0) GTEST_SKIP() << dev << " not available";

    Device d(dev);
    EXPECT_TRUE(d.open());
    EXPECT_TRUE(d.is_open());
}

// UT-LIB-002: 無効パスでopen()失敗
TEST(DeviceOpen, InvalidDeviceReturnsFalse) {
    Device d("/dev/spidevXX.0");
    EXPECT_FALSE(d.open());
    EXPECT_FALSE(d.is_open());
}

// UT-LIB-003: MockSpiDriver を使ってread()の返値を検証（実機不要）
TEST(DeviceRead, MockReturnsExpectedData) {
    MockSpiDriver mock;
    // transfer() が呼ばれたら 5 バイト成功を返すよう設定
    EXPECT_CALL(mock, open(_)).WillOnce(Return(true));
    EXPECT_CALL(mock, is_open()).WillRepeatedly(Return(true));
    EXPECT_CALL(mock, transfer(_, _, _)).WillOnce(Return(5));

    Device d(&mock);
    d.open();
    auto result = d.read(0x00, 4);
    EXPECT_EQ(result.size(), 4u);
}

// UT-LIB-004: 未open状態でread()は空vectorを返す
TEST(DeviceRead, NotOpenReturnsEmpty) {
    MockSpiDriver mock;
    EXPECT_CALL(mock, is_open()).WillRepeatedly(Return(false));
    EXPECT_CALL(mock, transfer(_, _, _)).WillOnce(Return(-1));

    Device d(&mock);
    auto result = d.read(0x00, 4);
    EXPECT_TRUE(result.empty());
}

// UT-LIB-005: write()正常系（MockSpiDriver使用）
TEST(DeviceWrite, MockWriteReturnsTrue) {
    MockSpiDriver mock;
    EXPECT_CALL(mock, open(_)).WillOnce(Return(true));
    EXPECT_CALL(mock, is_open()).WillRepeatedly(Return(true));
    EXPECT_CALL(mock, transfer(_, _, _)).WillOnce(Return(3));

    Device d(&mock);
    d.open();
    EXPECT_TRUE(d.write(0x10, {0xAA, 0xBB}));
}

// UT-LIB-006: 未open状態でwrite()はfalseを返す
TEST(DeviceWrite, NotOpenReturnsFalse) {
    MockSpiDriver mock;
    EXPECT_CALL(mock, transfer(_, _, _)).WillOnce(Return(-1));

    Device d(&mock);
    EXPECT_FALSE(d.write(0x10, {0xAA, 0xBB}));
}

// UT-LIB-007: read_async() でコールバックが呼ばれる（実機のみ）
TEST(DeviceReadAsync, CallbackIsCalled) {
    const char* dev = "/dev/spidev0.0";
    if (access(dev, F_OK) != 0) GTEST_SKIP();

    Device d(dev);
    ASSERT_TRUE(d.open());

    std::mutex mtx;
    std::condition_variable cv;
    bool done = false;
    int  cb_err = -1;
    size_t cb_size = 0;

    d.read_async(0x00, 4, [&](const std::vector<uint8_t>& data, int err) {
        // スレッド内の EXPECT は使わず、値を保存して外で検証する
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

// UT-LIB-008: 未openでread_async() → コールバックにerrが来るか空dataが来る
TEST(DeviceReadAsync, NotOpenCallbackReceivesError) {
    Device d("/dev/spidev0.0");   // 存在しないデバイス

    std::mutex mtx;
    std::condition_variable cv;
    bool   done          = false;
    int    received_err  = 0;
    size_t received_size = 99;   // 初期値を非ゼロにして「何も書かれなかった」を検出

    d.read_async(0x00, 4, [&](const std::vector<uint8_t>& data, int err) {
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
    EXPECT_TRUE(done);
    // 未openなので「データが空」または「errが非ゼロ」のどちらかが成立する
    EXPECT_TRUE(received_size == 0 || received_err != 0);
}

// UT-LIB-009: コピー禁止確認
TEST(DeviceCopyable, IsNotCopyConstructible) {
    EXPECT_FALSE(std::is_copy_constructible<Device>::value);
    EXPECT_FALSE(std::is_copy_assignable<Device>::value);
}
