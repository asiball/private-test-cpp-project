#pragma once
#include "../../driver/include/spi_driver.hpp"
#include <gmock/gmock.h>

namespace embedded {

class MockSpiDriver : public SpiDriver {
public:
    explicit MockSpiDriver() : SpiDriver("/dev/null") {}

    MOCK_METHOD(bool, open,     (const Config&), (override));
    MOCK_METHOD(void, close,    (),              (override));
    MOCK_METHOD(int,  transfer, (const uint8_t*, uint8_t*, size_t), (override));
    MOCK_METHOD(bool, is_open,  (), (const, override));
    MOCK_METHOD(int,  last_errno, (), (const, override));
};

} // namespace embedded
