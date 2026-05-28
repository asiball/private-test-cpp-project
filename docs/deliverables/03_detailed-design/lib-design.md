# 詳細設計書 — libdevice（Device クラス）

| 項目 | 内容 |
|---|---|
| ドキュメント番号 | DES-LIB-001 |
| バージョン | 1.1 |
| 作成日 | 2025-06-06 |
| 作成者 | 山田 太郎 |

---

## 1. クラス概要

`Device` はSPIデバイスへの**高レベルアクセス**を提供するクラスである。
内部ではPIMPLイディオムを使って実装詳細を隠蔽し、
コンストラクタ引数によって実機用・テスト用ドライバを切り替えられる設計になっている。

### 主な設計上の特徴

| 特徴 | 実現方法 |
|---|---|
| ABI安定性 | PIMPLイディオム（`Device::Impl` を `unique_ptr` で保持） |
| テスト可能性 | `ISpiDriver*` を外部から注入できるコンストラクタを持つ |
| 非同期対応 | `std::thread` + `detach` で `read_async()` を実装 |
| コピー禁止 | ファイルディスクリプタを所有するため `= delete` で禁止 |

---

## 2. クラス図

```
┌──────────────────────────────────────────────────────┐
│                    Device                            │
├──────────────────────────────────────────────────────┤
│ - impl_ : unique_ptr<Impl>                           │
├──────────────────────────────────────────────────────┤
│ + Device(spi_path: string)    ← 実機用               │
│ + Device(driver: ISpiDriver*) ← テスト用             │
│ + ~Device()                                          │
│ + open()       : bool                                │
│ + close()      : void                                │
│ + read(reg, len) : vector<uint8_t>                   │
│ + write(reg, data) : bool                            │
│ + read_async(reg, len, cb) : void                    │
│ + is_open()    : bool                                │
└──────────────────────┬───────────────────────────────┘
                       │ unique_ptr
       ┌───────────────▼───────────────┐
       │         Device::Impl          │
       ├───────────────────────────────┤
       │ + driver      : ISpiDriver*   │
       │ + owns_driver : bool          │
       └───────────────┬───────────────┘
                       │ ポインタ（所有 or 非所有）
       ┌───────────────┴───────────────┐
       │                               │
┌──────▼──────────────┐  ┌────────────▼────────────────┐
│    SpiDriver        │  │      MockSpiDriver           │
│  （実機用）         │  │     （テスト用）              │
└─────────────────────┘  └─────────────────────────────┘
```

---

## 3. PIMPLイディオム

### 3.1 設計意図

```cpp
// device.hpp — ヘッダには前方宣言のみ
class Device {
    struct Impl;                       // 前方宣言
    std::unique_ptr<Impl> impl_;       // ポインタで保持
};

// device.cpp — 実装の詳細はここだけ
struct Device::Impl {
    ISpiDriver* driver;
    bool        owns_driver;
    ...
};
```

ヘッダに `SpiDriver` のインクルードが不要になるため:

1. **再コンパイル削減**: `spi_driver.hpp` を変更しても `device.hpp` をインクルードしたファイルを再コンパイルしなくてよい
2. **ABI安定化**: `Impl` の内容を変えても `Device` のバイナリサイズが変わらない
3. **依存隠蔽**: `<linux/spi/spidev.h>` をライブラリ利用者に露出しない

### 3.2 所有権の管理

`Impl` は `driver` ポインタの所有権を `owns_driver` フラグで管理する:

```
Device(spi_path) → Impl が SpiDriver を new → owns_driver = true
Device(driver*)  → Impl はドライバを借りる  → owns_driver = false
```

デストラクタでは `owns_driver == true` の場合のみ `delete` する。
これにより、実機用・テスト用で同一の Impl 構造体を使いながら所有権を正しく管理できる。

---

## 4. 依存注入（DI）の仕組み

```cpp
// 実機用: SpiDriver を内部で生成して所有する
explicit Device(const std::string& spi_path)
    : impl_(std::make_unique<Impl>(spi_path))  // Impl が SpiDriver を new
{}

// テスト用: ISpiDriver* を外部から受け取る（所有しない）
explicit Device(ISpiDriver* driver)
    : impl_(std::make_unique<Impl>(driver))    // Impl はポインタを借用
{}
```

テストでは `MockSpiDriver` を渡すことで実機なしに動作を検証できる:

```cpp
MockSpiDriver mock;
EXPECT_CALL(mock, open(_)).WillOnce(Return(true));
EXPECT_CALL(mock, transfer(_, _, _)).WillOnce(Return(5));

Device d(&mock);   // 実機不要
d.open();
auto result = d.read(0x00, 4);
EXPECT_EQ(result.size(), 4u);
```

---

## 5. read/write のプロトコル実装

### 5.1 read()

```
入力: reg（アドレス）, len（バイト数）
処理:
  1. tx バッファ（len+1 バイト）を確保、tx[0] = reg | 0x80（読み出しビット）
  2. rx バッファ（len+1 バイト）を確保
  3. SpiDriver::transfer(tx, rx, len+1) を呼び出す
  4. rx[1..end] を返す（rx[0] はアドレスバイトへのエコーなので捨てる）
出力: vector<uint8_t>（len バイト）。失敗時は空 vector
```

アドレスビット7（MSB）を 1 に設定することで「読み出し要求」とみなすSPIデバイス仕様に対応している
（詳細は [SPIハードウェアIF仕様書](../05_interface-spec/spi-hardware-if.md) 参照）。

### 5.2 write()

```
入力: reg（アドレス）, data（バイト列）
処理:
  1. tx バッファに [reg & 0x7F, data...] を格納（書き込みビット: bit7 = 0）
  2. rx バッファ（同サイズ）を確保
  3. SpiDriver::transfer(tx, rx, tx.size()) を呼び出す
出力: bool（成功/失敗）
```

---

## 6. read_async() のスレッド設計

### 6.1 実装

```cpp
void Device::read_async(uint8_t reg, size_t len, ReadCallback cb)
{
    std::thread([this, reg, len, cb]() {
        auto result = this->read(reg, len);
        int  err    = result.empty() ? impl_->driver->last_errno() : 0;
        cb(result, err);
    }).detach();
}
```

`std::thread::detach()` によりスレッドをデタッチし、呼び出し元はブロックしない。

### 6.2 ライフタイムの注意事項

**重要**: `read_async()` でデタッチしたスレッドは `Device` のデストラクタを待たない。

```
呼び出し元                スレッド
    │                        │
    ├─ read_async() ─────── start
    │   (すぐ返る)           │
    │                    read() 実行中
    ├─ Device が破棄される !!
    │                    cb(result, err)  ← this が dangling pointer!
```

**対策**: `Device` オブジェクトのライフタイムをコールバック完了まで呼び出し元が保証すること。
長期稼働デーモンでは `shared_ptr + enable_shared_from_this` の採用を検討すること。

### 6.3 スレッドセーフ性

`Device` はスレッドセーフではない。複数スレッドから同一インスタンスに `read()`/`write()` を
並行して呼び出す場合は呼び出し元でミューテックス管理を行うこと。

---

## 7. エラーハンドリング方針

| 状況 | 戻り値 |
|---|---|
| `read()` 失敗（未オープン、転送エラー）| 空 `vector` |
| `write()` 失敗 | `false` |
| `open()` 失敗 | `false` |
| `read_async()` エラー | コールバックの第2引数（err）に errno 値を渡す |

例外は使用しない（`SpiDriver` 側の `noexcept` 設計と整合させるため）。

---

## 8. シーケンス図（同期読み出し）

```
呼び出し元          Device          Device::Impl       ISpiDriver
    │                 │                  │                  │
    ├── read(0x00, 4) ──────────────────►│                  │
    │                 │                  │                  │
    │                 │  tx = [0x80, 0, 0, 0, 0]            │
    │                 │  rx = [0,    0, 0, 0, 0]            │
    │                 │                  │                  │
    │                 ├── transfer(tx, rx, 5) ─────────────►│
    │                 │                  │                  │
    │                 │◄─── 5（バイト数） ──────────────────┤
    │                 │                  │                  │
    │◄── rx[1..4] ────┤                  │                  │
```
