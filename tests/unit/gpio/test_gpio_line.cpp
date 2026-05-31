#include "gpio_line.hpp"
#include <gtest/gtest.h>
#include <cerrno>
#include <unistd.h>

using namespace embedded;

// UT-GPIO-001: request 前の wait_event() は -1 (EBADF)
TEST(GpioLineWait, NotRequestedReturnsMinusOne) {
    GpioLine line("/dev/gpiochip0", 17);
    EXPECT_EQ(line.wait_event(10), -1);
    EXPECT_EQ(line.last_errno(), EBADF);
}

// UT-GPIO-002: 存在しないチップで request_edge_events() 失敗
TEST(GpioLineRequest, InvalidChipReturnsFalse) {
    GpioLine line("/dev/gpiochip-no-such", 0);
    EXPECT_FALSE(line.request_edge_events(GpioLine::Edge::Rising));
    EXPECT_FALSE(line.is_requested());
    EXPECT_NE(line.last_errno(), 0);
}

// UT-GPIO-003: 二重 close は安全
TEST(GpioLineClose, DoubleCloseIsSafe) {
    GpioLine line("/dev/gpiochip-no-such", 0);
    EXPECT_NO_FATAL_FAILURE({
        line.close();
        line.close();
    });
}

// UT-GPIO-004: コピー禁止確認
TEST(GpioLineCopyable, IsNotCopyConstructible) {
    EXPECT_FALSE(std::is_copy_constructible<GpioLine>::value);
    EXPECT_FALSE(std::is_copy_assignable<GpioLine>::value);
}

// UT-GPIO-005: 実機の gpiochip でエッジ要求（実機のみ）
TEST(GpioLineRequest, ValidChipRequestsEvents) {
    const char* chip = "/dev/gpiochip0";
    if (access(chip, F_OK) != 0) GTEST_SKIP() << chip << " not available";

    GpioLine line(chip, 17);
    // 実機ではラインが他用途で使用中だと失敗しうるため、結果は緩く確認
    if (line.request_edge_events(GpioLine::Edge::Both)) {
        EXPECT_TRUE(line.is_requested());
        EXPECT_GE(line.event_fd(), 0);
    } else {
        GTEST_SKIP() << "line busy or unavailable";
    }
}
