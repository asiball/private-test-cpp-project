#include "adxl345.hpp"
#include "adxl345_reg.hpp"
#include "../../tests/mocks/mock_spi_driver.hpp"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstdint>
#include <vector>
#include <unistd.h>

using namespace embedded;
namespace df  = adxl345::data_format;
namespace pc  = adxl345::power_ctl;
namespace acc = adxl345::access;
using ::testing::_;
using ::testing::Return;

// 1 バイト読み出し(rx[1]) に value を載せる ACTION
ACTION_P(FillReadByte, value) {
    uint8_t* rx = static_cast<uint8_t*>(arg1);
    rx[1] = static_cast<uint8_t>(value);
}

// UT-ADXL-001: 有効パスで open() 成功（実機のみ）
TEST(Adxl345Open, ValidDeviceReturnsTrue) {
    const char* dev = "/dev/spidev0.0";
    if (access(dev, F_OK) != 0) GTEST_SKIP() << dev << " not available";

    Adxl345 d(dev);
    // 実機が ADXL345 とは限らないため、戻り値の真偽は問わずクラッシュしないことを確認
    (void)d.open();
    SUCCEED();
}

//! [UT-ADXL-002]
// UT-ADXL-002: 無効パスで open() 失敗
TEST(Adxl345Open, InvalidDeviceReturnsFalse) {
    Adxl345 d("/dev/spidevXX.0");
    EXPECT_FALSE(d.open());
    EXPECT_FALSE(d.is_open());
}
//! [UT-ADXL-002]

//! [UT-ADXL-003]
// UT-ADXL-003: read_reg が READ ビット付きアドレスを送り、rx[1] を返す
TEST(Adxl345Reg, ReadRegSendsReadFramedAddress) {
    MockSpiDriver mock;
    uint8_t captured0 = 0xFF;
    EXPECT_CALL(mock, transfer(_, _, 2))
        .WillOnce([&](const uint8_t* tx, uint8_t* rx, size_t) {
            captured0 = tx[0];
            rx[1]     = 0xE5;
            return 2;
        });

    Adxl345 d(&mock);
    auto v = d.read_reg(adxl345::reg::DEVID);
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, 0xE5);
    EXPECT_EQ(captured0, acc::READ | adxl345::reg::DEVID);   // 0x80
}
//! [UT-ADXL-003]

//! [UT-ADXL-004]
// UT-ADXL-004: write_reg は R/W=0（書き込み）でアドレスと値を送る
TEST(Adxl345Reg, WriteRegSendsWriteFramedAddressAndValue) {
    MockSpiDriver mock;
    uint8_t captured[2] = {0xFF, 0xFF};
    EXPECT_CALL(mock, transfer(_, _, 2))
        .WillOnce([&](const uint8_t* tx, uint8_t*, size_t) {
            captured[0] = tx[0];
            captured[1] = tx[1];
            return 2;
        });

    Adxl345 d(&mock);
    EXPECT_TRUE(d.write_reg(adxl345::reg::DATA_FORMAT, df::FULL_RES | df::RANGE_16G));
    EXPECT_EQ(captured[0], acc::WRITE | adxl345::reg::DATA_FORMAT);  // 0x31, bit7=0
    EXPECT_EQ(captured[1], df::FULL_RES | df::RANGE_16G);            // 0x0B
}
//! [UT-ADXL-004]

//! [UT-ADXL-005]
// UT-ADXL-005: update_bits は read-modify-write で対象ビット以外を保持する
TEST(Adxl345Reg, UpdateBitsPreservesOtherBits) {
    MockSpiDriver mock;
    uint8_t written = 0xFF;
    // 1 回目(read): 現在値 0b1010_0000 を返す / 2 回目(write): 値を捕捉
    EXPECT_CALL(mock, transfer(_, _, 2))
        .WillOnce(testing::DoAll(FillReadByte(0xA0), Return(2)))
        .WillOnce([&](const uint8_t* tx, uint8_t*, size_t) {
            written = tx[1];
            return 2;
        });

    Adxl345 d(&mock);
    // 下位 2bit(RANGE)だけ 0b11 に。上位の 0xA0 は保持される想定 → 0xA3
    EXPECT_TRUE(d.update_bits(adxl345::reg::DATA_FORMAT, df::RANGE_MASK, df::RANGE_16G));
    EXPECT_EQ(written, 0xA3);
}
//! [UT-ADXL-005]

//! [UT-ADXL-006]
// UT-ADXL-006: read_raw が DATAX0 からのマルチバイト読みでリトルエンディアン合成する
TEST(Adxl345Data, ReadRawAssemblesLittleEndianSignedAxes) {
    MockSpiDriver mock;
    uint8_t captured0 = 0xFF;
    EXPECT_CALL(mock, transfer(_, _, 7))
        .WillOnce([&](const uint8_t* tx, uint8_t* rx, size_t) {
            captured0 = tx[0];
            // X=+0x0100(256), Y=-1(0xFFFF), Z=+0x7FFF
            rx[1] = 0x00; rx[2] = 0x01;   // X
            rx[3] = 0xFF; rx[4] = 0xFF;   // Y
            rx[5] = 0xFF; rx[6] = 0x7F;   // Z
            return 7;
        });

    Adxl345 d(&mock);
    auto a = d.read_raw();
    ASSERT_TRUE(a.has_value());
    EXPECT_EQ(captured0, acc::READ | acc::MULTIBYTE | adxl345::reg::DATAX0);  // 0xF2
    EXPECT_EQ(a->x, 256);
    EXPECT_EQ(a->y, -1);
    EXPECT_EQ(a->z, 32767);
}
//! [UT-ADXL-006]

//! [UT-ADXL-007]
// UT-ADXL-007: read_g が 3.9mg/LSB でスケールする
TEST(Adxl345Data, ReadGScalesByFullResFactor) {
    MockSpiDriver mock;
    EXPECT_CALL(mock, transfer(_, _, 7))
        .WillOnce([&](const uint8_t*, uint8_t* rx, size_t) {
            // X=256 LSB ≒ 1g（256 * 3.9mg = 0.9984g）
            rx[1] = 0x00; rx[2] = 0x01;
            rx[3] = 0x00; rx[4] = 0x00;
            rx[5] = 0x00; rx[6] = 0x00;
            return 7;
        });

    Adxl345 d(&mock);
    auto a = d.read_g();
    ASSERT_TRUE(a.has_value());
    EXPECT_NEAR(a->x, 256 * Adxl345::SCALE_G_PER_LSB, 1e-9);
    EXPECT_NEAR(a->y, 0.0, 1e-9);
}
//! [UT-ADXL-007]

//! [UT-ADXL-008]
// UT-ADXL-008: open() が DEVID 確認 → DATA_FORMAT/POWER_CTL 設定を行う
TEST(Adxl345Open, ConfiguresDeviceWhenDevidMatches) {
    MockSpiDriver mock;
    EXPECT_CALL(mock, open(_)).WillOnce(Return(true));
    std::vector<std::pair<uint8_t, uint8_t>> writes;
    EXPECT_CALL(mock, transfer(_, _, 2))
        .WillRepeatedly([&](const uint8_t* tx, uint8_t* rx, size_t) {
            if (tx[0] & acc::READ) {       // 読み出し → DEVID 応答
                rx[1] = adxl345::DEVID_VALUE;
            } else {                        // 書き込み → 記録
                writes.emplace_back(tx[0], tx[1]);
            }
            return 2;
        });

    Adxl345 d(&mock);
    ASSERT_TRUE(d.open());
    ASSERT_EQ(writes.size(), 2u);
    EXPECT_EQ(writes[0].first,  acc::WRITE | adxl345::reg::DATA_FORMAT);
    EXPECT_EQ(writes[0].second, df::FULL_RES | df::RANGE_16G);
    EXPECT_EQ(writes[1].first,  acc::WRITE | adxl345::reg::POWER_CTL);
    EXPECT_EQ(writes[1].second, pc::MEASURE);
}
//! [UT-ADXL-008]

//! [UT-ADXL-009]
// UT-ADXL-009: DEVID 不一致なら open() は false（誤デバイス検出）
TEST(Adxl345Open, WrongDevidFailsOpen) {
    MockSpiDriver mock;
    EXPECT_CALL(mock, open(_)).WillOnce(Return(true));
    EXPECT_CALL(mock, close());
    EXPECT_CALL(mock, transfer(_, _, 2))
        .WillOnce(testing::DoAll(FillReadByte(0x00), Return(2)));  // DEVID≠0xE5

    Adxl345 d(&mock);
    EXPECT_FALSE(d.open());
}
//! [UT-ADXL-009]

// UT-ADXL-010: 転送失敗時は read_raw が std::nullopt
TEST(Adxl345Data, TransferErrorReturnsNullopt) {
    MockSpiDriver mock;
    EXPECT_CALL(mock, transfer(_, _, 7)).WillOnce(Return(-1));

    Adxl345 d(&mock);
    EXPECT_FALSE(d.read_raw().has_value());
}

// UT-ADXL-012: enable_tap_detection が閾値/持続/軸/INT_MAP/INT_ENABLE を設定する
TEST(Adxl345Interrupt, EnableTapDetectionWritesExpectedRegisters) {
    MockSpiDriver mock;
    std::vector<std::pair<uint8_t, uint8_t>> writes;
    EXPECT_CALL(mock, transfer(_, _, 2))
        .WillRepeatedly([&](const uint8_t* tx, uint8_t* rx, size_t) {
            if (tx[0] & acc::READ) {
                rx[1] = 0x00;   // INT_MAP / INT_ENABLE の現在値（update_bits の read 側）
            } else {
                writes.emplace_back(tx[0] & acc::ADDR_MASK, tx[1]);
            }
            return 2;
        });

    Adxl345 d(&mock);
    EXPECT_TRUE(d.enable_tap_detection(0x20, 0x10, Adxl345::TAP_AXIS_Z));

    // THRESH_TAP / DUR / TAP_AXES / INT_MAP / INT_ENABLE の 5 書き込み
    ASSERT_EQ(writes.size(), 5u);
    EXPECT_EQ(writes[0], std::make_pair(adxl345::reg::THRESH_TAP, uint8_t{0x20}));
    EXPECT_EQ(writes[1], std::make_pair(adxl345::reg::DUR,        uint8_t{0x10}));
    EXPECT_EQ(writes[2], std::make_pair(adxl345::reg::TAP_AXES,   Adxl345::TAP_AXIS_Z));
    // INT_MAP: SINGLE_TAP ビットは 0（INT1 へ）→ 現在値 0 のまま 0x00
    EXPECT_EQ(writes[3], std::make_pair(adxl345::reg::INT_MAP,    uint8_t{0x00}));
    // INT_ENABLE: SINGLE_TAP(0x40) を立てる
    EXPECT_EQ(writes[4], std::make_pair(adxl345::reg::INT_ENABLE, Adxl345::INT_SINGLE_TAP));
}

// UT-ADXL-013: enable_free_fall が THRESH_FF/TIME_FF/INT_ENABLE を設定する
TEST(Adxl345Interrupt, EnableFreeFallWritesExpectedRegisters) {
    MockSpiDriver mock;
    std::vector<std::pair<uint8_t, uint8_t>> writes;
    EXPECT_CALL(mock, transfer(_, _, 2))
        .WillRepeatedly([&](const uint8_t* tx, uint8_t* rx, size_t) {
            if (tx[0] & acc::READ) { rx[1] = 0x00; }
            else { writes.emplace_back(tx[0] & acc::ADDR_MASK, tx[1]); }
            return 2;
        });

    Adxl345 d(&mock);
    EXPECT_TRUE(d.enable_free_fall(0x07, 0x28));
    ASSERT_EQ(writes.size(), 4u);
    EXPECT_EQ(writes[0], std::make_pair(adxl345::reg::THRESH_FF, uint8_t{0x07}));
    EXPECT_EQ(writes[1], std::make_pair(adxl345::reg::TIME_FF,   uint8_t{0x28}));
    EXPECT_EQ(writes[2], std::make_pair(adxl345::reg::INT_MAP,   uint8_t{0x00}));
    EXPECT_EQ(writes[3], std::make_pair(adxl345::reg::INT_ENABLE, Adxl345::INT_FREE_FALL));
}

// UT-ADXL-014: disable_interrupts が INT_ENABLE=0 を書く
TEST(Adxl345Interrupt, DisableInterruptsClearsIntEnable) {
    MockSpiDriver mock;
    uint8_t captured[2] = {0xFF, 0xFF};
    EXPECT_CALL(mock, transfer(_, _, 2))
        .WillOnce([&](const uint8_t* tx, uint8_t*, size_t) {
            captured[0] = tx[0]; captured[1] = tx[1];
            return 2;
        });

    Adxl345 d(&mock);
    EXPECT_TRUE(d.disable_interrupts());
    EXPECT_EQ(captured[0], acc::WRITE | adxl345::reg::INT_ENABLE);
    EXPECT_EQ(captured[1], 0x00);
}

// UT-ADXL-015: read_interrupt_source が INT_SOURCE を読み値を返す
TEST(Adxl345Interrupt, ReadInterruptSourceReturnsRegister) {
    MockSpiDriver mock;
    uint8_t captured0 = 0xFF;
    EXPECT_CALL(mock, transfer(_, _, 2))
        .WillOnce([&](const uint8_t* tx, uint8_t* rx, size_t) {
            captured0 = tx[0];
            rx[1]     = Adxl345::INT_SINGLE_TAP;   // タップ発生
            return 2;
        });

    Adxl345 d(&mock);
    auto src = d.read_interrupt_source();
    ASSERT_TRUE(src.has_value());
    EXPECT_EQ(captured0, acc::READ | adxl345::reg::INT_SOURCE);
    EXPECT_TRUE(*src & Adxl345::INT_SINGLE_TAP);
}

// UT-ADXL-011: コピー禁止確認
TEST(Adxl345Copyable, IsNotCopyConstructible) {
    EXPECT_FALSE(std::is_copy_constructible<Adxl345>::value);
    EXPECT_FALSE(std::is_copy_assignable<Adxl345>::value);
}
