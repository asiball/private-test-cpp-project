# 詳細設計書 — libsensor（Sensor / Ads1115）

> 本書は libsensor の 2 製品を扱う。§1〜§8 は `Sensor`（MCP3008 / SPI）、§9 は `Ads1115`（ADS1115 / I2C, 任意ターゲット）。

| 項目 | 内容 |
|---|---|
| ドキュメント番号 | DES-LIB-001 |
| バージョン | 1.1 |
| 作成日 | 2025-06-06 |
| 作成者 | 山田 太郎 |

---

## 1. クラス概要

`Sensor` はSPIデバイスへの**高レベルアクセス**を提供するクラスである。
内部ではPIMPLイディオムを使って実装詳細を隠蔽し、
コンストラクタ引数によって実機用・テスト用ドライバを切り替えられる設計になっている。

### 主な設計上の特徴

| 特徴 | 実現方法 |
|---|---|
| ABI安定性 | PIMPLイディオム（`Sensor::Impl` を `unique_ptr` で保持） |
| テスト可能性 | `ISpiDriver*` を外部から注入できるコンストラクタを持つ |
| 非同期対応 | `std::thread` を生成し、デタッチせず `Impl` が保持してデストラクタで `join` する方式で `read_raw_async()` を実装 |
| コピー禁止 | ファイルディスクリプタを所有するため `= delete` で禁止 |

---

## 2. クラス図

```mermaid
classDiagram
    class Sensor {
        -unique_ptr~Impl~ impl_
        +Sensor(spi_path, vref) 実機用
        +Sensor(driver, vref) テスト用
        +~Sensor()
        +open() bool
        +close() void
        +is_open() bool
        +read_raw(channel) optional~uint16_t~
        +read_voltage(channel) optional~double~
        +read_raw_async(channel, cb) void
        +vref() double
        +set_vref(v) void
    }
    class Impl["Sensor::Impl"] {
        +owned unique_ptr~ISpiDriver~
        +driver ISpiDriver
        +workers vector~thread~
        +io_mutex mutex
    }
    class ISpiDriver {
        <<interface>>
    }
    class SpiDriver["SpiDriver（実機用）"]
    class MockSpiDriver["MockSpiDriver（テスト用）"]
    Sensor *-- Impl : unique_ptr
    Impl ..> ISpiDriver : driver（所有 or 非所有）
    ISpiDriver <|.. SpiDriver
    ISpiDriver <|.. MockSpiDriver
```

---

## 3. PIMPLイディオム

### 3.1 設計意図

```cpp
// sensor.hpp — ヘッダには前方宣言のみ
class Sensor {
    struct Impl;                       // 前方宣言
    std::unique_ptr<Impl> impl_;       // ポインタで保持
};

// sensor.cpp — 実装の詳細はここだけ
struct Sensor::Impl {
    std::unique_ptr<ISpiDriver> owned;   // 自前生成時のみ所有（注入時は空）
    ISpiDriver*                 driver;  // 実際に使う非所有ビュー
    ...
};
```

ヘッダに `SpiDriver` のインクルードが不要になるため:

1. **再コンパイル削減**: `spi_driver.hpp` を変更しても `sensor.hpp` をインクルードしたファイルを再コンパイルしなくてよい
2. **ABI安定化**: `Impl` の内容を変えても `Sensor` のバイナリサイズが変わらない
3. **依存隠蔽**: `<linux/spi/spidev.h>` をライブラリ利用者に露出しない

### 3.2 所有権の管理

`Impl` は `driver` ポインタの所有権を `std::unique_ptr<ISpiDriver> owned` で管理する:

```
Sensor(spi_path) → Impl が SpiDriver を make_unique で生成 → owned が所有、driver = owned.get()
Sensor(driver*)  → Impl はドライバを借りる               → owned は空、driver = 借用ポインタ
```

デストラクタでは `owned` が非 null の場合のみ（`unique_ptr` の通常のセマンティクスで）自動的に解放される。
これにより、実機用・テスト用で同一の Impl 構造体を使いながら所有権を正しく管理でき、手動 `delete` や
所有権フラグの管理漏れによるリークを構造的に防いでいる。

---

## 4. 依存注入（DI）の仕組み

```cpp
// 実機用: SpiDriver を内部で生成して所有する
explicit Sensor(const std::string& spi_path)
    : impl_(std::make_unique<Impl>(spi_path))  // Impl が SpiDriver を new
{}

// テスト用: ISpiDriver* を外部から受け取る（所有しない）
explicit Sensor(ISpiDriver* driver)
    : impl_(std::make_unique<Impl>(driver))    // Impl はポインタを借用
{}
```

テストでは `MockSpiDriver` を渡すことで実機なしに動作を検証できる:

```cpp
MockSpiDriver mock;
EXPECT_CALL(mock, open(_)).WillOnce(Return(true));
// CH0 から raw=512 が返るよう rx[1]=0x02, rx[2]=0x00 を注入する
EXPECT_CALL(mock, transfer(_, _, 3)).WillOnce(DoAll(FillMcp3008Rx(512), Return(3)));

Sensor s(&mock);   // 実機不要
s.open();
auto raw = s.read_raw(0);
ASSERT_TRUE(raw.has_value());
EXPECT_EQ(*raw, 512u);
```

---

## 5. MCP3008 読み出しプロトコル実装

MCP3008 はシングルエンド／差動入力に対応した 8ch・10bit SPI ADC である。
1 回の読み出しは **3 バイトのフルデュプレクス転送**で完結する
（詳細は [SPIハードウェアIF仕様書](../05_interface-spec/spi-hardware-if.md) 参照）。

### 5.1 read_raw()

```
入力: channel（0〜7）
処理:
  1. channel が CHANNEL_COUNT(8) 以上なら nullopt を返す
  2. tx[3] を構築する:
        tx[0] = 0x01                    （スタートビット）
        tx[1] = 0x80 | (channel << 4)   （シングルエンド + チャネル選択）
        tx[2] = 0x00                    （クロック供給用ダミー）
  3. ISpiDriver::transfer(tx, rx, 3) を呼び出す
  4. raw = ((rx[1] & 0x03) << 8) | rx[2]   （下位 10bit を組み立てる）
出力: optional<uint16_t>（0〜1023）。失敗時は nullopt
```

### 5.2 read_voltage()

```
入力: channel（0〜7）
処理:
  1. raw = read_raw(channel)
  2. raw が nullopt なら nullopt を返す
  3. voltage = raw * vref() / ADC_MAX を返す
出力: optional<double>（電圧 [V]）。失敗時は nullopt
```

---

## 6. read_raw_async() のスレッド設計

### 6.1 実装

```cpp
void Sensor::read_raw_async(uint8_t channel, ReadCallback cb)
{
    if (channel >= CHANNEL_COUNT) {
        cb(std::nullopt, EINVAL);
        return;
    }
    // ワーカーは detach せず impl_->workers に保持し、~Impl で join する。
    std::lock_guard<std::mutex> lk(impl_->workers_mtx);
    impl_->workers.emplace_back([this, channel, cb]() {
        std::optional<uint16_t> result;
        int err = 0;
        {
            std::lock_guard<std::mutex> io_lk(impl_->io_mutex);
            result = mcp3008_transfer_locked(impl_->driver, channel);
            if (!result) {
                err = impl_->driver->last_errno();
            }
        }
        cb(result, err);
    });
}
```

生成したワーカースレッドはデタッチせず `Sensor::Impl::workers`（`std::vector<std::thread>`）に
保持し、`Impl` のデストラクタで全て `join` する。呼び出し元は `read_raw_async()` 呼び出し自体では
ブロックされないが、`Sensor` の破棄（デストラクタ）は未完了のコールバックを待ち合わせるため
ブロックしうる。

### 6.2 ライフタイムの安全性

デタッチ方式（旧実装）では、コールバック完了前に `Sensor` が破棄されると
ワーカースレッドが保持する `this` がダングリングポインタになり use-after-free を起こしうる
という問題があった。現在は join 方式に変更しており、この問題は構造的に発生しない。

```mermaid
sequenceDiagram
    participant Caller as 呼び出し元
    participant Sensor
    participant Worker as ワーカースレッド
    Caller->>Sensor: read_raw_async()（すぐ返る）
    Sensor->>Worker: emplace_back(...)（workers に保持）
    activate Worker
    Note over Worker: io_mutex を取得して transfer() 実行中
    Caller->>Sensor: ~Sensor()（→ ~Impl()）
    Note over Sensor: workers の各スレッドを join して待機
    Worker->>Worker: cb(result, err)
    deactivate Worker
    Note over Sensor: join 完了 → Impl / driver を破棄
    Sensor-->>Caller: ~Sensor() から復帰
```

**設計上の帰結**: `Sensor` オブジェクトの生存期間中に発行した `read_raw_async()` のコールバックは、
`this` が有効な間にのみ実行される（join によって保証される）。そのため呼び出し元がコールバック完了まで
`Sensor` の生存を明示的に保証する必要はないが、破棄処理（デストラクタ）が完了中のコールバックの
分だけブロックしうる点、および長期稼働デーモンで大量に発行すると完了済みスレッドが
`workers` に蓄積しうる点には注意すること。

### 6.3 スレッドセーフ性

`driver->transfer()` の呼び出しから `last_errno()` の読み出しまでは `Impl::io_mutex` で
直列化されており、`read_raw()` / `read_voltage()` / `read_raw_async()` を複数スレッドから
同一インスタンスに対して並行に呼び出してもデータレースにはならない。ただし `open()` / `close()` や
`set_vref()` を含めた操作順序の一貫性（例: `open()` 完了前に読み出さない）は呼び出し元の責務である。

---

## 7. エラーハンドリング方針

| 状況 | 戻り値 |
|---|---|
| `read_raw()` 失敗（未オープン、範囲外チャネル、転送エラー）| `std::nullopt` |
| `read_voltage()` 失敗 | `std::nullopt` |
| `open()` 失敗 | `false` |
| `read_raw_async()` エラー | コールバックの第2引数（err）に errno 値を渡す |

例外は使用しない（`SpiDriver` 側の `noexcept` 設計と整合させるため）。

---

## 8. シーケンス図（同期読み出し）

```mermaid
sequenceDiagram
    participant Caller as 呼び出し元
    participant Sensor
    participant Impl as Sensor::Impl
    participant Driver as ISpiDriver
    Caller->>Sensor: read_raw(0)
    Sensor->>Impl: read_raw(0)
    Note over Impl: tx = [0x01, 0x80, 0x00]<br/>rx = [0, 0, 0]
    Impl->>Driver: transfer(tx, rx, 3)
    Driver-->>Impl: 3（バイト数）
    Impl-->>Sensor: raw
    Sensor-->>Caller: raw = ((rx[1] & 0x03) << 8) | rx[2]
```

---

## 9. Ads1115（I2C, 任意ターゲット）

`Ads1115` は ADS1115（4ch / 16bit I2C ADC）への高レベルアクセス。`Sensor`（MCP3008 / SPI）と対をなし、**`II2cDriver` への DI + PIMPL** という同じ設計パターンで作る。`i2c-hal` がある場合のみビルドされる独立ターゲット（`-lads1115`）。レジスタ詳細は [IF-ADS-001](../05_interface-spec/ads1115-register-map.md)。

### 9.1 クラス構成 / 依存注入

`Impl` は `std::unique_ptr<II2cDriver> owned` / `II2cDriver* driver` / `uint16_t addr` / `Gain gain` / `bool rdy_pin_enabled` を持つ。`Ads1115(path, addr)` は `I2cDriver` を `make_unique` で生成し `owned` に所有させる、`Ads1115(II2cDriver*, addr)` は `owned` を空にして借用（テストでは `MockI2cDriver` を注入）。所有時の解放は `unique_ptr` のデストラクタに任せるため手動 `delete` は不要。16bit レジスタ I/O はヘルパで行う:

| ヘルパ | 動作 |
|---|---|
| `write_reg16(reg, val)` | `[ reg, MSB, LSB ]` の 3 バイトを `driver->write()` |
| `read_reg16(reg, out)` | `driver->write_read(&reg, 1, rx, 2)`（リピーテッドスタート）|

### 9.2 read_raw() — シングルショット + OS ポーリング

```
入力: channel(0〜3)。範囲外は nullopt
1. Config を組み立てる:
   OS_SINGLE | (MUX_SINGLE | channel<<12) | pga_bits(gain) | MODE_SINGLE | DR_128SPS | comp_que
   comp_que = rdy_pin_enabled ? COMP_QUE_ONE(0x0000) : COMP_QUE_DISABLE(0x0003)
2. write_reg16(CONFIG, config)        // OS=1 で変換開始
3. 変換完了待ち: read_reg16(CONFIG) の OS(bit15)==1 まで（最大16回・1ms間隔）
4. read_reg16(CONVERSION) → (int16_t)
出力: optional<int16_t>
```

### 9.3 read_voltage() / ゲイン

`read_voltage = raw * full_scale(gain) / 32768`。`set_gain` / `gain` / `full_scale_volts` で PGA（±6.144V〜±0.256V）を管理する。

### 9.4 enable_conversion_ready_pin()

`Hi_thresh(0x03)=0x8000` / `Lo_thresh(0x02)=0x0000` を書き、`rdy_pin_enabled=true` にする。以降の `read_raw` の Config は `COMP_QUE_ONE` を使い、変換完了で ALERT/RDY をアサートさせる。GPIO 割り込み（[DES-GPIO-001](gpio-design.md)）と組み合わせると、OS ポーリングの代わりに割り込み駆動で読み出せる。

### 9.5 スレッド安全性

スレッドセーフではない。`Sensor` と異なり非同期 API は持たず、割り込み駆動は `GpioLine` 側で実現する。
