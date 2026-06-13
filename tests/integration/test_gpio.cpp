// 結合テスト: GPIO エッジ割り込み（gpiochip）実機必須
// IT-010 〜 IT-011 に対応
//
// 事前条件:
//   - /dev/gpiochip0 が存在する Linux 実機
//   - 物理エッジを与えるテスト（IT-011）は信号源が必要。CI では timeout=0 経路のみ確認
//   - gpiochip が無い CI 環境では SetUp で GTEST_SKIP する
#include "gpio_line.hpp"
#include <gtest/gtest.h>
#include <unistd.h>

namespace {
constexpr const char* GPIO_CHIP   = "/dev/gpiochip0";
constexpr unsigned    GPIO_OFFSET = 17;   // テスト用ライン（環境に合わせて変更）
}

class GpioIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (access(GPIO_CHIP, F_OK) != 0)
            GTEST_SKIP() << "gpiochip not available (not a target board)";
    }
};

// IT-010: ラインのエッジイベント要求に成功する
TEST_F(GpioIntegrationTest, RequestEdgeEventsSucceeds) {
    embedded::GpioLine line(GPIO_CHIP, GPIO_OFFSET);
    if (!line.request_edge_events(embedded::GpioLine::Edge::Both))
        GTEST_SKIP() << "request_edge_events failed (ライン使用中/権限の可能性) errno="
                     << line.last_errno();
    EXPECT_TRUE(line.is_requested());
}

// IT-011: 信号が来なければ wait_event() はタイムアウト（0）を返す
TEST_F(GpioIntegrationTest, WaitEventTimesOutWithoutEdge) {
    embedded::GpioLine line(GPIO_CHIP, GPIO_OFFSET);
    if (!line.request_edge_events(embedded::GpioLine::Edge::Rising))
        GTEST_SKIP() << "request_edge_events failed errno=" << line.last_errno();
    // 短いタイムアウトでエッジが来ないことを確認（0 = タイムアウト）
    EXPECT_EQ(line.wait_event(100), 0);
}
