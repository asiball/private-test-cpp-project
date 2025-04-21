#include "device.hpp"
#include <gtest/gtest.h>
#include <condition_variable>
#include <mutex>

using namespace embedded;

// UT-LIB-001: 有効パスでopen()成功
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

// UT-LIB-004: 未open状態でread()は空vectorを返す
TEST(DeviceRead, NotOpenReturnsEmpty) {
    Device d("/dev/spidev0.0");
    auto result = d.read(0x00, 4);
    EXPECT_TRUE(result.empty());
}

// UT-LIB-006: 未open状態でwrite()はfalseを返す
TEST(DeviceWrite, NotOpenReturnsFalse) {
    Device d("/dev/spidev0.0");
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

    d.read_async(0x00, 4, [&](const std::vector<uint8_t>& data, int err) {
        EXPECT_EQ(err, 0);
        EXPECT_EQ(data.size(), 4u);
        {
            std::lock_guard<std::mutex> lk(mtx);
            done = true;
        }
        cv.notify_one();
    });

    std::unique_lock<std::mutex> lk(mtx);
    cv.wait_for(lk, std::chrono::seconds(3), [&]{ return done; });
    EXPECT_TRUE(done) << "callback was not called within 3 seconds";
}

// UT-LIB-008: 未openでread_async() → コールバックにerrが来る
TEST(DeviceReadAsync, NotOpenCallbackReceivesError) {
    Device d("/dev/spidev0.0");

    std::mutex mtx;
    std::condition_variable cv;
    bool done = false;
    int received_err = 0;

    d.read_async(0x00, 4, [&](const std::vector<uint8_t>& data, int err) {
        received_err = err;
        {
            std::lock_guard<std::mutex> lk(mtx);
            done = true;
        }
        cv.notify_one();
    });

    std::unique_lock<std::mutex> lk(mtx);
    cv.wait_for(lk, std::chrono::seconds(3), [&]{ return done; });
    EXPECT_TRUE(done);
    EXPECT_TRUE(data.empty() || received_err != 0);
}

// UT-LIB-009: コピー禁止確認
TEST(DeviceCopyable, IsNotCopyConstructible) {
    EXPECT_FALSE(std::is_copy_constructible<Device>::value);
    EXPECT_FALSE(std::is_copy_assignable<Device>::value);
}
