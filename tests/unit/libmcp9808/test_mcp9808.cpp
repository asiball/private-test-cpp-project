#include "mcp9808.hpp"
#include "../../tests/mocks/mock_i2c_driver.hpp"
#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace embedded;
using ::testing::_;
using ::testing::Return;

namespace {
// write_read のモック: reg に応じた 2 バイトを MSB first で返す
auto MakeReg16Fake(uint8_t reg_match, uint16_t value) {
    return [reg_match, value](const uint8_t* tx, size_t, uint8_t* rx, size_t rx_len) -> int {
        if (rx_len < 2) return -1;
        if (tx[0] == reg_match) {
            rx[0] = static_cast<uint8_t>(value >> 8);
            rx[1] = static_cast<uint8_t>(value & 0xFF);
        } else {
            rx[0] = 0x00;
            rx[1] = 0x00;
        }
        return static_cast<int>(rx_len);
    };
}
} // namespace

// UT-MCP-001: 無効パスで open() 失敗（実機ドライバ経由）
TEST(Mcp9808Open, InvalidDeviceReturnsFalse) {
    Mcp9808 dev("/dev/i2cXX");
    EXPECT_FALSE(dev.open());
    EXPECT_FALSE(dev.is_open());
}

// UT-MCP-002: open() が製造者 ID(0x0054) を確認して成功する
TEST(Mcp9808Open, VerifiesManufacturerId) {
    MockI2cDriver mock;
    EXPECT_CALL(mock, open(Mcp9808::DEFAULT_ADDR)).WillOnce(Return(true));
    EXPECT_CALL(mock, write_read(_, _, _, _))
        .WillRepeatedly(MakeReg16Fake(0x06, Mcp9808::MANUFACTURER_ID));  // MANUF_ID

    Mcp9808 dev(&mock);
    EXPECT_TRUE(dev.open());
}

// UT-MCP-003: 製造者 ID 不一致なら open() は false（誤デバイス検出）
TEST(Mcp9808Open, WrongManufacturerIdFailsOpen) {
    MockI2cDriver mock;
    EXPECT_CALL(mock, open(_)).WillOnce(Return(true));
    EXPECT_CALL(mock, close());
    EXPECT_CALL(mock, write_read(_, _, _, _))
        .WillRepeatedly(MakeReg16Fake(0x06, 0x1234));  // 誤った ID

    Mcp9808 dev(&mock);
    EXPECT_FALSE(dev.open());
}

// UT-MCP-004: 正の温度を 0.0625°C 分解能で換算する（+25.25°C）
TEST(Mcp9808Temp, ReadsPositiveTemperature) {
    MockI2cDriver mock;
    // +25.25°C → 25.25/0.0625 = 404 = 0x0194。上位 0x01, 下位 0x94
    EXPECT_CALL(mock, write_read(_, _, _, _))
        .WillRepeatedly(MakeReg16Fake(0x05, 0x0194));  // T_AMBIENT

    Mcp9808 dev(&mock);
    auto t = dev.read_temperature();
    ASSERT_TRUE(t.has_value());
    EXPECT_NEAR(*t, 25.25, 1e-9);
}

// UT-MCP-005: フラグビット(15:13)が立っていても温度部のみを解釈する
TEST(Mcp9808Temp, IgnoresFlagBits) {
    MockI2cDriver mock;
    // 0x0194 にフラグビット 0xE000 を付与 → 温度は +25.25°C のまま
    EXPECT_CALL(mock, write_read(_, _, _, _))
        .WillRepeatedly(MakeReg16Fake(0x05, 0xE194));

    Mcp9808 dev(&mock);
    auto t = dev.read_temperature();
    ASSERT_TRUE(t.has_value());
    EXPECT_NEAR(*t, 25.25, 1e-9);
}

// UT-MCP-006: 負の温度を符号ビットで正しく換算する（-1.0°C）
TEST(Mcp9808Temp, ReadsNegativeTemperature) {
    MockI2cDriver mock;
    // -1.0°C: 符号ビット(0x1000) を立て、温度部は 256-1=255°C 相当の表現。
    // upper=0x1F, lower=0xF0 → (0x0F)*16 + 0xF0*0.0625 = 240 + 15 = 255, 符号で -256 → -1.0
    EXPECT_CALL(mock, write_read(_, _, _, _))
        .WillRepeatedly(MakeReg16Fake(0x05, 0x1FF0));

    Mcp9808 dev(&mock);
    auto t = dev.read_temperature();
    ASSERT_TRUE(t.has_value());
    EXPECT_NEAR(*t, -1.0, 1e-9);
}

// UT-MCP-007: 転送失敗時は read_temperature が std::nullopt
TEST(Mcp9808Temp, TransferErrorReturnsNullopt) {
    MockI2cDriver mock;
    EXPECT_CALL(mock, write_read(_, _, _, _)).WillOnce(Return(-1));

    Mcp9808 dev(&mock);
    EXPECT_FALSE(dev.read_temperature().has_value());
}

// UT-MCP-008: コピー禁止確認
TEST(Mcp9808Copyable, IsNotCopyConstructible) {
    EXPECT_FALSE(std::is_copy_constructible<Mcp9808>::value);
    EXPECT_FALSE(std::is_copy_assignable<Mcp9808>::value);
}
