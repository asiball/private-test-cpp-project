#pragma once
#include "../../driver/include/ispi_driver.hpp"
#include <gmock/gmock.h>

namespace embedded {

/**
 * @brief ISpiDriver のテスト用モック実装
 *
 * SpiDriver（実機依存）の代わりにテストで使用する。
 * ISpiDriver を継承しているため、Device::Impl に差し込める。
 */
class MockSpiDriver : public ISpiDriver {
public:
    MOCK_METHOD(bool, open,       (const Config&),                      (override));
    MOCK_METHOD(void, close,      (),                                   (override));
    MOCK_METHOD(int,  transfer,   (const uint8_t*, uint8_t*, size_t),   (override));
    MOCK_METHOD(bool, is_open,    (),                                   (const, override));
    MOCK_METHOD(int,  last_errno, (),                                   (const, override));
};

} // namespace embedded
