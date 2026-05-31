#pragma once
#include "../../i2c-hal/include/ii2c_driver.hpp"
#include <gmock/gmock.h>

namespace embedded {

/**
 * @brief II2cDriver のテスト用モック実装
 *
 * I2cDriver（実機依存）の代わりにテストで使用する。
 * II2cDriver を継承しているため、Ads1115 に差し込める。
 */
class MockI2cDriver : public II2cDriver {
public:
    MOCK_METHOD(bool, open,       (uint16_t),                                  (noexcept, override));
    MOCK_METHOD(void, close,      (),                                          (noexcept, override));
    MOCK_METHOD(int,  write,      (const uint8_t*, size_t),                    (noexcept, override));
    MOCK_METHOD(int,  read,       (uint8_t*, size_t),                          (noexcept, override));
    MOCK_METHOD(int,  write_read, (const uint8_t*, size_t, uint8_t*, size_t),  (noexcept, override));
    MOCK_METHOD(bool, is_open,    (),                                          (const, noexcept, override));
    MOCK_METHOD(int,  last_errno, (),                                          (const, noexcept, override));
};

} // namespace embedded
