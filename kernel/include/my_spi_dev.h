/**
 * @file my_spi_dev.h
 * @brief カーネルモジュール my_spi_driver とユーザー空間で共有する ioctl 定義
 *
 * このヘッダはカーネルとユーザー空間の両方からインクルードできる。
 * カーネル側は __KERNEL__ が定義されるため、それぞれのヘッダを使い分ける。
 */
#pragma once

#ifdef __KERNEL__
#include <linux/ioctl.h>
#include <linux/types.h>
#else
#include <sys/ioctl.h>
#include <stdint.h>
#endif

/** @brief ioctl マジックナンバー */
#define MY_SPI_IOC_MAGIC 'M'

/**
 * @brief SPI 設定パラメータ（open 時に渡す）
 */
struct my_spi_config {
    uint32_t speed_hz;      /**< クロック周波数 [Hz] */
    uint8_t  bits_per_word; /**< ワードビット幅（通常 8） */
    uint8_t  mode;          /**< SPI モード 0〜3 */
};

/**
 * @brief SPI フルデュプレクス転送パラメータ
 *
 * tx_buf/rx_buf にはユーザー空間のポインタを uint64_t にキャストして渡す。
 * 32/64 ビット環境の互換性のため uint64_t を使用する。
 */
struct my_spi_transfer {
    uint64_t tx_buf; /**< 送信バッファのユーザー空間アドレス */
    uint64_t rx_buf; /**< 受信バッファのユーザー空間アドレス */
    uint32_t len;    /**< 転送バイト数 */
};

/** @brief SPI パラメータ設定（write-only ioctl） */
#define MY_SPI_IOC_CONFIG   _IOW(MY_SPI_IOC_MAGIC,  1, struct my_spi_config)

/** @brief フルデュプレクス SPI 転送（read/write ioctl） */
#define MY_SPI_IOC_TRANSFER _IOWR(MY_SPI_IOC_MAGIC, 2, struct my_spi_transfer)
