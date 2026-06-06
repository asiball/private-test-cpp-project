# SPI バスと MCP3008（10bit ADC）

> 本プロジェクトの**土台**となる題材です。SPI バスの基本から、Linux の `spidev` 越しに
> MCP3008（8ch / 10bit ADC）を読むまでを、一般的な技術として紹介します。
> 具体例として本リポジトリの `spi-hal/` と `libsensor/`（`Sensor` クラス）を参照します。
> ここを押さえると、[I2C と ADS1115](i2c-and-ads1115.md) や [GPIO 割り込みと epoll](gpio-interrupts-epoll.md)
> が「同じ枠組みの引き直し」として読めるようになります。

---

## 1. SPI とは（バスの基本）

**SPI（Serial Peripheral Interface）** は、1 つのマスタ（CPU/SoC）と 1 つ以上のスレーブ（センサ等）を
**4 本の信号線**で結ぶ同期式シリアルバスです。

| 信号 | 向き | 役割 |
|---|---|---|
| **SCLK** | マスタ→スレーブ | クロック（マスタが供給）|
| **MOSI** | マスタ→スレーブ | Master Out / Slave In（送信データ）|
| **MISO** | スレーブ→マスタ | Master In / Slave Out（受信データ）|
| **CS（SS）** | マスタ→スレーブ | チップセレクト（通信相手を選ぶ。通常 Low 有効）|

特徴は **フルデュプレクス**であること。クロック 1 拍ごとに、MOSI で 1bit 送ると同時に MISO で 1bit 受け取ります。
つまり「N バイト送る」と「N バイト受け取る」が**常に同時**に起きます（後述の MCP3008 はこの性質をそのまま使います）。

### SPI モード（CPOL / CPHA）

クロックの極性（CPOL）と位相（CPHA）の組み合わせで 4 つのモードがあります。
スレーブのデータシートが要求するモードに合わせます。

| モード | CPOL | CPHA | 備考 |
|---|---|---|---|
| 0 | 0 | 0 | 最も一般的。**MCP3008 はこれ** |
| 1 | 0 | 1 | |
| 2 | 1 | 0 | |
| 3 | 1 | 1 | |

> I2C との比較（信号線数・アドレス指定・全/半二重など）は [I2C と ADS1115 §1](i2c-and-ads1115.md) の表が対になっています。

---

## 2. Linux で SPI を使う（spidev）

Linux ではユーザ空間から **`/dev/spidevX.Y`**（X=バス番号, Y=CS番号）を開き、
`ioctl` で設定・転送します。本プロジェクトの薄いラッパが `spi-hal/src/spi_driver.cpp` の `SpiDriver` です。

流れは 2 段階です。

**① オープン時に SPI パラメータを設定**（`SpiDriver::open`）:

```cpp
fd_ = ::open(device_path_.c_str(), O_RDWR);
ioctl(fd_, SPI_IOC_WR_MODE,          &cfg.mode);          // モード 0〜3
ioctl(fd_, SPI_IOC_WR_BITS_PER_WORD, &cfg.bits_per_word); // 通常 8
ioctl(fd_, SPI_IOC_WR_MAX_SPEED_HZ,  &cfg.speed_hz);      // クロック上限
```

**② 転送は `SPI_IOC_MESSAGE` で 1 回の `ioctl` にまとめる**（`SpiDriver::transfer`）:

```cpp
struct spi_ioc_transfer tr = {};
tr.tx_buf = reinterpret_cast<uintptr_t>(tx);  // 送信バッファ
tr.rx_buf = reinterpret_cast<uintptr_t>(rx);  // 受信バッファ（同じ長さ）
tr.len    = len;
ioctl(fd_, SPI_IOC_MESSAGE(1), &tr);          // 1 メッセージを送受信
```

ここで **`tx` と `rx` が同じ長さ**なのがフルデュプレクスの現れです。CS の上げ下げは
カーネルの spidev ドライバが 1 メッセージの前後で自動的に行います。

> 実装では `tr.speed_hz = 0` / `tr.bits_per_word = 0` を渡し、「この転送は `open()` で設定した
> デバイス既定値を使う」ことを明示しています。また `EAGAIN`（一時的なリソース不足）は最大 3 回リトライします。
> 物理配線・タイミングの詳細仕様は [SPI ハードウェア IF 仕様](../deliverables/05_interface-spec/spi-hardware-if.md)（成果物）にあります。

---

## 3. MCP3008 のプロトコル

**MCP3008** は SPI 接続の **8 チャネル / 10bit** ADC です。シングルエンドモードでは、
**3 バイトを送りながら 3 バイトを受け取る**だけで 1 チャネルを読めます。

### 3 バイト転送の中身（`Sensor::read_raw`）

```cpp
// channel は 0〜7
uint8_t tx[3] = { 0x01,                       // ① スタートビット
                  0x80 | (channel << 4),      // ② SGL/DIFF=1(single) + チャネル選択
                  0x00 };                     // ③ クロックを供給するだけのダミー
uint8_t rx[3] = { 0, 0, 0 };
driver.transfer(tx, rx, 3);

// rx[1] の下位 2bit が変換結果の bit9-8、rx[2] が bit7-0
uint16_t raw = ((rx[1] & 0x03) << 8) | rx[2]; // 0〜1023（10bit）
```

| バイト | TX（送る）| RX（同時に受け取る）|
|---|---|---|
| 0 | `0x01`（スタートビット）| 不定 |
| 1 | `0x80 \| (ch<<4)`（モード＋ch）| 上位 6bit は不定、**下位 2bit = 結果の bit9-8** |
| 2 | `0x00`（ダミー）| **結果の bit7-0** |

「送信しながら受信する」フルデュプレクスなので、ダミーの `0x00` を送るのは
**変換結果を取り出すためのクロックを供給する**目的です。

### 電圧への換算

10bit のフルスケールは 1023（`ADC_MAX`）なので:

```
voltage = raw * Vref / 1023      // Vref は既定 3.3V（set_vref で変更可）
```

> クロックは `Sensor::open()` で **1 MHz**（Vdd=3.3V 時の MCP3008 上限 1.35 MHz 未満）、モード 0、8bit に設定しています。

---

## 4. 設計：インターフェース + PIMPL + 依存注入（DI）

このコンポーネントは、本プロジェクトの設計パターンの**最小の縦串**になっています。

- **`ISpiDriver`**（[`spi-hal/include/ispi_driver.hpp`](../../spi-hal/include/ispi_driver.hpp)）
  … 純粋仮想インターフェース。`open(Config)` / `transfer(tx, rx, len)` / `close()` を `[[nodiscard]] noexcept` で定義。
  実機 `SpiDriver` とテスト用 `MockSpiDriver` が同じ型として扱える。
- **`Sensor`**（[`libsensor/include/sensor.hpp`](../../libsensor/include/sensor.hpp)）
  … MCP3008 の高レベル API。**PIMPL** で実装を隠し、コンストラクタで `ISpiDriver*` を
  受け取れる（**DI**）ので、実機なしに単体テストできる。

```cpp
// テストでは本物の spidev の代わりにモックを差し込む（実機不要）
MockSpiDriver mock;
EXPECT_CALL(mock, transfer(_, _, 3))
    .WillOnce(DoAll(FillMcp3008Rx(0x2A5), Return(3)));
embedded::Sensor s(&mock);
EXPECT_EQ(*s.read_raw(0), 0x2A5);
```

> ここで使われている各パターンの一般的な解説は
> [C++設計パターンガイド](tooling/cpp-patterns-guide.md)、テスト/モックの書き方は
> [Google Test / GMock ガイド](tooling/gtest-guide.md) を参照してください。
> C のイディオム（不透明ポインタ・ops 表・`goto err`）との対比は
> [C→C++ ステップアップガイド](c-to-cpp-stepping-stones.md) にまとめています。

---

## 5. 使い方

```cpp
#include "sensor.hpp"

embedded::Sensor sensor("/dev/spidev0.0");   // 既定 Vref=3.3V
if (!sensor.open()) return;

if (auto v = sensor.read_voltage(0)) {        // CH0 の電圧
    std::cout << "CH0 = " << *v << " V\n";
}
```

非同期版 `read_raw_async(channel, cb)` は内部で `std::thread` を起動して完了時に `cb` を呼びます。
**コールバック完了まで `Sensor` を生かしておく**のは呼び出し側の責任です（長期稼働デーモンでは
`shared_ptr` + `enable_shared_from_this` を検討。サニタイザーでの検出例は
[サニタイザーガイド](tooling/sanitizers-guide.md) 参照）。

対話的に試すには CLI ツール `device-ctl` が使えます:

```bash
./build/cli/device-ctl -d /dev/spidev0.0 --vref 3.3
```

---

## 次の一歩

- [I2C と ADS1115](i2c-and-ads1115.md) — 別バス・16bit で「同じ ADC を読む」引き直し
- [GPIO 割り込みと epoll](gpio-interrupts-epoll.md) — ポーリングをイベント駆動に置き換える
- [C→C++ ステップアップガイド](c-to-cpp-stepping-stones.md) ・ [C++設計パターンガイド](tooling/cpp-patterns-guide.md)
