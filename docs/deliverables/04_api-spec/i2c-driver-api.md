# API仕様書 — I2cDriver / II2cDriver

| 項目 | 内容 |
|---|---|
| ドキュメント番号 | API-I2C-001 |
| バージョン | 1.0 |
| 作成日 | 2026-06-01 |
| ヘッダ | `#include <i2c_driver.hpp>` / `#include <ii2c_driver.hpp>` |
| リンク | `-li2chal` |
| 名前空間 | `embedded::` |

> Linux の i2c-dev（`/dev/i2c-N`）を介した I2C 通信の低レベルドライバ。
> spi-hal（`ISpiDriver`）と対になる存在で、上位（ADS1115 等）を実機から切り離す。
> **SPI と違い I2C はハーフデュプレクス**のため、`transfer()` 1 本ではなく `write()` / `read()` / `write_read()` に分かれる。

---

## 1. II2cDriver クラス（抽象インターフェース）

`II2cDriver` は I2C ドライバの抽象インターフェース。実機用実装（`I2cDriver`）とテスト用モック（`MockI2cDriver`）が共通の型として扱われる。`Ads1115` は `II2cDriver*` のみを参照するため、実機なしでのテストが可能になる。

### 1.1 メソッド一覧

| メソッド | 戻り値 | 説明 |
|---|---|---|
| `open(addr)` | `bool` | バスをオープンし、通信相手の 7bit スレーブアドレスを設定する |
| `close()` | `void` | バスをクローズする。未オープン時は no-op |
| `write(data, len)` | `int` | スレーブへ `len` バイト書き込む。書き込んだバイト数、エラー時 -1 |
| `read(data, len)` | `int` | スレーブから `len` バイト読み出す。読み出したバイト数、エラー時 -1 |
| `write_read(tx, tx_len, rx, rx_len)` | `int` | 書き込み→リピーテッドスタート→読み出しを1トランザクションで実行。読み出したバイト数、エラー時 -1 |
| `is_open()` | `bool` | バスがオープン中なら true |
| `last_errno()` | `int` | 直近エラーの errno 値 |

> `write_read()` は「レジスタ番号を書く → STOP せずに読み出しに切り替える → 読む」というレジスタ参照の定石（例：ADS1115 の変換結果読み出し）を 1 トランザクションで行う。途中で STOP しないため、他マスタの割り込みを避けられる。

---

## 2. I2cDriver クラス

Linux i2c-dev を介した I2C 通信ドライバ（実機用）。`II2cDriver` を実装し、`/dev/i2c-N` の `open` / `ioctl` / `read` / `write` / `close` を管理する。

コピー禁止（RAII）。スレッドセーフではない。

### 2.1 コンストラクタ / デストラクタ

```cpp
explicit I2cDriver(const std::string& device_path);
~I2cDriver();  // open 中なら自動的に close する

I2cDriver(const I2cDriver&)            = delete;
I2cDriver& operator=(const I2cDriver&) = delete;
```

| パラメータ | 説明 |
|---|---|
| `device_path` | i2c-dev のデバイスパス（例: `"/dev/i2c-1"`）|

### 2.2 open()

```cpp
[[nodiscard]] bool open(uint16_t addr) noexcept;
```

**説明**: `addr` が 7bit 範囲を超えていないか検証してから `::open(device_path, O_RDWR)` でバスをオープンし、`ioctl(I2C_SLAVE, addr)` で以降の `read`/`write` の宛先となるスレーブアドレスを設定する。

| パラメータ | 説明 |
|---|---|
| `addr` | 7bit スレーブアドレス（`0x00`〜`0x7F`。例: ADS1115 = `0x48`）。範囲外は `ioctl` を呼ばず `EINVAL` で拒否 |

| 戻り値 | 条件 |
|---|---|
| `true` | オープン成功 |
| `false` | 既にオープン済み、`addr` が 7bit 範囲（`0x00`〜`0x7F`）外（`EINVAL`）、デバイスが存在しない、権限不足、`I2C_SLAVE` 失敗 |

### 2.3 write() / read()

```cpp
[[nodiscard]] int write(const uint8_t* data, size_t len) noexcept;
[[nodiscard]] int read(uint8_t* data, size_t len) noexcept;
```

**説明**: 設定済みのスレーブへ `::write` / `::read` する。`len == 0` は何もせず `0` を返す。

| 戻り値 | 条件 |
|---|---|
| `>= 0` | 成功。実際に転送したバイト数 |
| `-1` | 未オープン（`EBADF`）、`data == nullptr`（`EINVAL`）、I/O エラー |

### 2.4 write_read()

```cpp
[[nodiscard]] int write_read(const uint8_t* tx, size_t tx_len,
                             uint8_t* rx, size_t rx_len) noexcept;
```

**説明**: `I2C_RDWR` を用い、リピーテッドスタートを伴う結合トランザクションを実行する。

```
[START] addr+W, tx... [REPEATED START] addr+R, rx... [STOP]
```

| パラメータ | 説明 |
|---|---|
| `tx` / `tx_len` | 書き込みバイト列（通常はレジスタポインタ）とその長さ |
| `rx` / `rx_len` | 読み出しバッファとその長さ |

| 戻り値 | 条件 |
|---|---|
| `rx_len` | 成功（読み出したバイト数）|
| `-1` | 未オープン（`EBADF`）、ポインタ NULL / 長さ 0（`EINVAL`）、長さが `UINT16_MAX` 超過（`EOVERFLOW`）、`I2C_RDWR` 失敗 |

---

## 3. 使用例

```cpp
#include <i2c_driver.hpp>
#include <cstdio>

int main()
{
    embedded::I2cDriver bus("/dev/i2c-1");
    if (!bus.open(0x48)) {            // ADS1115 (ADDR=GND)
        fprintf(stderr, "open failed: errno=%d\n", bus.last_errno());
        return 1;
    }

    uint8_t reg = 0x00;              // Conversion レジスタ
    uint8_t rx[2] = {};
    if (bus.write_read(&reg, 1, rx, 2) < 0) {
        fprintf(stderr, "write_read failed: errno=%d\n", bus.last_errno());
        return 1;
    }
    int16_t raw = static_cast<int16_t>((rx[0] << 8) | rx[1]);
    printf("conversion raw = %d\n", raw);
    return 0;
    // bus のデストラクタが自動的に close() を呼ぶ（RAII）
}
```

---

## 4. エラーハンドリング方針

- エラー発生時は `last_errno_` に `errno` を保存し、`last_errno()` で参照できる。
- **例外は使用しない**（組み込み環境を考慮した `noexcept` 設計）。失敗は戻り値（`-1` / `false`）で表す。
- 代表的な errno: `EBADF`（未オープン）/ `EINVAL`（NULL・長さ0）/ `EOVERFLOW`（長さ超過）。

## 5. スレッド安全性

`I2cDriver` はスレッドセーフではない。複数スレッドから同一インスタンスを使う場合は呼び出し側でミューテックス管理を行うこと。

## 6. バージョン履歴

| バージョン | 変更内容 |
|---|---|
| 1.0 | 初版。`open`, `close`, `write`, `read`, `write_read`, `is_open`, `last_errno` |
