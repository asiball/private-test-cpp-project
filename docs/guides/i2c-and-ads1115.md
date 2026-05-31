# I2C バスと ADS1115（16bit ADC）

> 既存の SPI / MCP3008 と対になる教材。「同じ ADC という仕事を、別のバス・別の分解能で」
> 実装することで、**バス抽象の引き直し**と **I2C プロトコル**を学びます。
> 各コンポーネントは独立しており、I2C を使わないビルドでも他は成立します。

---

## 1. SPI と I2C の違い（まず全体像）

| | SPI（spi-hal）| I2C（i2c-hal）|
|---|---|---|
| 信号線 | SCLK / MOSI / MISO / CS（4本＋）| SDA / SCL（2本）|
| 転送 | フルデュプレクス（送受信同時）| ハーフデュプレクス（write / read を分ける）|
| 相手の選択 | CS ピン | 7bit スレーブアドレス |
| Linux デバイス | `/dev/spidevX.Y` | `/dev/i2c-N` |
| 主な ioctl | `SPI_IOC_MESSAGE` | `I2C_SLAVE` / `I2C_RDWR` |

この違いがインターフェースに表れます。`ISpiDriver::transfer(tx, rx, len)`（同時送受信）に対し、
`II2cDriver` は `write()` / `read()` / `write_read()` を分けています。

---

## 2. I2cDriver — Linux i2c-dev の薄いラッパ

`i2c-hal/src/i2c_driver.cpp`。流れは SPI ドライバと同型です:

1. `open()` … `/dev/i2c-N` を `::open` し、`ioctl(fd, I2C_SLAVE, addr)` で相手を設定
2. `write()` / `read()` … 設定済みの相手へ `::write` / `::read`
3. `write_read()` … **リピーテッドスタート**を伴う結合トランザクション（`I2C_RDWR`）

### なぜ write_read（リピーテッドスタート）が要るのか

レジスタを持つデバイス（ADS1115 など）から読むときは、
**「レジスタ番号を書く → STOP せずに read に切り替える → 読む」** が定石です。
途中で STOP すると別のマスタに割り込まれる余地ができるため、`I2C_RDWR` で 2 メッセージを
1 トランザクションにまとめます:

```
[START] addr+W, reg [REPEATED START] addr+R, data... [STOP]
```

```cpp
struct i2c_msg msgs[2];
msgs[0] = { addr_, 0,        tx_len, const_cast<uint8_t*>(tx) }; // 書き込み
msgs[1] = { addr_, I2C_M_RD, rx_len, rx };                       // 読み出し
struct i2c_rdwr_ioctl_data xfer { msgs, 2 };
ioctl(fd_, I2C_RDWR, &xfer);
```

---

## 3. ADS1115 のプロトコル

ADS1115 は 4 レジスタを持つ 16bit ADC です。

| レジスタ | ポインタ | 用途 |
|---|---|---|
| Conversion | 0x00 | 変換結果（符号付き 16bit）|
| Config | 0x01 | 変換開始・チャネル・ゲイン等 |
| Lo_thresh | 0x02 | コンパレータ下限 / RDY 用 |
| Hi_thresh | 0x03 | コンパレータ上限 / RDY 用 |

### シングルショット読み出しの手順（`Ads1115::read_raw`）

1. **Config を書く**（変換開始）: `OS=1`, `MUX`=シングルエンドの対象 ch, `PGA`=ゲイン, `MODE=1`(single-shot)
2. **変換完了を待つ**: Config を読み、`OS` ビットが 1 に戻れば完了（= ポーリング）
3. **Conversion を読む**: 16bit 値を取得

Config（16bit）の組み立て例（CH0, ±2.048V, 128SPS）:

```
OS(0x8000) | MUX_A0(0x4000) | PGA_2.048V(0x0400) | MODE_single(0x0100)
          | DR_128SPS(0x0080) | COMP_QUE_disable(0x0003)  = 0xC583
```

この `0xC583` は単体テスト `UT-ADS-006`（`tests/unit/libsensor/test_ads1115.cpp`）で検証しています。

### 電圧への換算

符号付き 16bit のフルスケールは 32768 なので:

```
voltage = raw * full_scale_volts() / 32768
```

`full_scale_volts()` は `set_gain()` で選んだ PGA に対応します（±6.144V〜±0.256V）。

---

## 4. MCP3008 との対比（学習ポイント）

| | MCP3008 (SPI) | ADS1115 (I2C) |
|---|---|---|
| 分解能 | 10bit (0〜1023) | 16bit（符号付き）|
| 読み出し | 3 バイト転送 1 回 | Config 書込 → 完了待ち → Conversion 読出 |
| レンジ | Vref 固定 | PGA で可変 |
| クラス | `Sensor`（ISpiDriver 依存）| `Ads1115`（II2cDriver 依存）|

**同じ "ADC を読む" でも、バスとデバイスの作法が変わると上位 API の形も変わる**ことが体感できます。
両者とも PIMPL + 依存性注入（DI）という同じ設計パターンで作られている点にも注目してください。

---

## 5. 使い方

```cpp
#include "ads1115.hpp"

embedded::Ads1115 adc("/dev/i2c-1");   // ADDR=GND → 0x48
if (!adc.open()) return;
adc.set_gain(embedded::Ads1115::Gain::FSR_4_096V);
if (auto v = adc.read_voltage(0)) {
    std::cout << "ch0 = " << *v << " V\n";
}
```

ALERT/RDY ピンを使った割り込み駆動の読み出しは
[GPIO 割り込みと epoll](gpio-interrupts-epoll.md) を参照してください。

関連: [C→C++ ステップアップガイド](c-to-cpp-stepping-stones.md)
