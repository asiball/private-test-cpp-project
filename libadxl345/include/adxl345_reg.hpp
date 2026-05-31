#pragma once
#include <cstdint>

/**
 * @file adxl345_reg.hpp
 * @brief ADXL345（3軸加速度センサ）レジスタマップ定義
 *
 * このヘッダは ADXL345 の「レジスタマップ」をコード化したもの。
 * IF 仕様書 docs/deliverables/05_interface-spec/adxl345-register-map.md の
 * レジスタ表・ビットフィールド図と 1:1 で対応する（レビューで突き合わせ可能）。
 *
 * MCP3008（レジスタを持たないコマンド型 ADC）と異なり、ADXL345 は
 * アドレス指定でレジスタを読み書きするデバイス。各設定はビット単位で行う。
 *
 * SPI アクセスは先頭の「アドレスバイト」で R/W・連続転送・アドレスを指定する:
 * @code
 *   bit7  R/W      : 1=読み出し / 0=書き込み           (access::READ / WRITE)
 *   bit6  MB       : 1=連続(マルチバイト)転送           (access::MULTIBYTE)
 *   bit5:0 アドレス : 対象レジスタ(0x00〜0x39)            (access::ADDR_MASK)
 * @endcode
 */

namespace embedded::adxl345 {

/** @brief レジスタアドレス（本実装で使用するもの） */
namespace reg {
inline constexpr uint8_t DEVID       = 0x00; ///< R   デバイス ID（固定値 0xE5）
inline constexpr uint8_t BW_RATE     = 0x2C; ///< R/W 出力データレート / 低電力モード
inline constexpr uint8_t POWER_CTL   = 0x2D; ///< R/W 電源・測定制御
inline constexpr uint8_t DATA_FORMAT = 0x31; ///< R/W データフォーマット（分解能・レンジ等）
inline constexpr uint8_t DATAX0      = 0x32; ///< R   X 軸 LSB（以降 6 バイト連続: X0,X1,Y0,Y1,Z0,Z1）
inline constexpr uint8_t DATAX1      = 0x33; ///< R   X 軸 MSB
inline constexpr uint8_t DATAY0      = 0x34; ///< R   Y 軸 LSB
inline constexpr uint8_t DATAY1      = 0x35; ///< R   Y 軸 MSB
inline constexpr uint8_t DATAZ0      = 0x36; ///< R   Z 軸 LSB
inline constexpr uint8_t DATAZ1      = 0x37; ///< R   Z 軸 MSB
} // namespace reg

/** @brief DEVID(0x00) のリセット時固定値。open 時の疎通確認に使う */
inline constexpr uint8_t DEVID_VALUE = 0xE5;

/** @brief SPI アクセスフレーミング（アドレスバイトの上位 2bit） */
namespace access {
inline constexpr uint8_t READ      = 0x80; ///< bit7=1: 読み出し
inline constexpr uint8_t WRITE     = 0x00; ///< bit7=0: 書き込み
inline constexpr uint8_t MULTIBYTE = 0x40; ///< bit6=1: 連続（マルチバイト）転送
inline constexpr uint8_t ADDR_MASK = 0x3F; ///< bit5:0: レジスタアドレス
} // namespace access

/**
 * @brief POWER_CTL(0x2D) ビットフィールド
 *
 * ```
 *  7      6      5      4         3        2      1     0
 * ┌─────┬─────┬─────┬──────────┬────────┬──────┬───────────┐
 * │  0  │  0  │LINK │AUTO_SLEEP│MEASURE │SLEEP │  WAKEUP    │
 * └─────┴─────┴─────┴──────────┴────────┴──────┴───────────┘
 * ```
 */
namespace power_ctl {
inline constexpr uint8_t LINK        = 1 << 5; ///< Link 機能
inline constexpr uint8_t AUTO_SLEEP  = 1 << 4; ///< 自動スリープ
inline constexpr uint8_t MEASURE     = 1 << 3; ///< 0=待機(standby) / 1=測定(measure)
inline constexpr uint8_t SLEEP       = 1 << 2; ///< スリープ
inline constexpr uint8_t WAKEUP_MASK = 0x03;   ///< bit1:0 スリープ時のサンプリング周波数
} // namespace power_ctl

/**
 * @brief DATA_FORMAT(0x31) ビットフィールド
 *
 * ```
 *  7         6        5         4    3        2        1     0
 * ┌─────────┬────────┬─────────┬───┬────────┬────────┬───────────┐
 * │SELF_TEST│SPI 3wire│INT_INV │ 0 │FULL_RES│JUSTIFY │  RANGE    │
 * └─────────┴────────┴─────────┴───┴────────┴────────┴───────────┘
 *   RANGE: 00=±2g, 01=±4g, 10=±8g, 11=±16g
 * ```
 */
namespace data_format {
inline constexpr uint8_t SELF_TEST  = 1 << 7; ///< セルフテスト
inline constexpr uint8_t SPI_3WIRE  = 1 << 6; ///< 1=3線式 / 0=4線式 SPI
inline constexpr uint8_t INT_INVERT = 1 << 5; ///< 割り込み極性反転
inline constexpr uint8_t FULL_RES   = 1 << 3; ///< 1=フル分解能（3.9mg/LSB 固定）/ 0=10bit 固定
inline constexpr uint8_t JUSTIFY    = 1 << 2; ///< 1=左詰め(MSB) / 0=右詰め(符号拡張)
inline constexpr uint8_t RANGE_MASK = 0x03;   ///< bit1:0 測定レンジ
inline constexpr uint8_t RANGE_2G   = 0x00;   ///< ±2 g
inline constexpr uint8_t RANGE_4G   = 0x01;   ///< ±4 g
inline constexpr uint8_t RANGE_8G   = 0x02;   ///< ±8 g
inline constexpr uint8_t RANGE_16G  = 0x03;   ///< ±16 g
} // namespace data_format

/**
 * @brief BW_RATE(0x2C) ビットフィールド
 *
 * ```
 *  7   6   5    4         3     2     1     0
 * ┌───┬───┬───┬──────────┬─────────────────────┐
 * │ 0 │ 0 │ 0 │LOW_POWER │       RATE          │
 * └───┴───┴───┴──────────┴─────────────────────┘
 * ```
 */
namespace bw_rate {
inline constexpr uint8_t LOW_POWER   = 1 << 4; ///< 低電力モード
inline constexpr uint8_t RATE_MASK   = 0x0F;   ///< bit3:0 出力データレート
inline constexpr uint8_t RATE_100HZ  = 0x0A;   ///< 100 Hz（リセット既定値）
} // namespace bw_rate

} // namespace embedded::adxl345
