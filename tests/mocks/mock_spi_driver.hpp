#pragma once
#include "../../spi-hal/include/ispi_driver.hpp"
#include <gmock/gmock.h>

namespace embedded {

/**
 * @brief ISpiDriver のテスト用モック実装
 *
 * SpiDriver（実機依存）の代わりにテストで使用する。
 * ISpiDriver を継承しているため、Sensor::Impl に差し込める。
 */
class MockSpiDriver : public ISpiDriver {
public:
    MOCK_METHOD(bool, open,       (const Config&),                      (noexcept, override));
    MOCK_METHOD(void, close,      (),                                   (noexcept, override));
    MOCK_METHOD(int,  transfer,   (const uint8_t*, uint8_t*, size_t),   (noexcept, override));
    MOCK_METHOD(bool, is_open,    (),                                   (const, noexcept, override));
    MOCK_METHOD(int,  last_errno, (),                                   (const, noexcept, override));
};

} // namespace embedded
