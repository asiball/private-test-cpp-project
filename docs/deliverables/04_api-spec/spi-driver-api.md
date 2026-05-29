# API仕様書 — SpiDriver / ISpiDriver

| 項目 | 内容 |
|---|---|
| ドキュメント番号 | API-DRV-001 |
| バージョン | 1.1 |
| ヘッダ | `#include <spi_driver.hpp>` / `#include <ispi_driver.hpp>` |
| リンク | `-lspihal` |
| 名前空間 | `embedded::` |

---

## 1. ISpiDriver クラス（抽象インターフェース）

`ISpiDriver` はSPIドライバの抽象インターフェースである。
実機用実装（`SpiDriver`）とテスト用モック（`MockSpiDriver`）が共通の型として扱われる。
`Sensor` クラスは `ISpiDriver*` のみを参照するため、実機なしでのテストが可能になる。

### 1.1 Config 構造体

```cpp
struct ISpiDriver::Config {
    uint32_t speed_hz;      // クロック周波数 [Hz]（例: 1000000 = 1MHz）
    uint8_t  bits_per_word; // ワードビット幅（通常 8）
    uint8_t  mode;          // SPI モード 0〜3（MODE 0 = CPOL=0, CPHA=0）
};
```

| フィールド | 型 | 説明 |
|---|---|---|
| `speed_hz` | `uint32_t` | クロック周波数。最大 2,000,000（2MHz）|
| `bits_per_word` | `uint8_t` | ワードビット幅。通常 8 |
| `mode` | `uint8_t` | SPI モード。0〜3（MODE 0 = CPOL=0, CPHA=0）|

### 1.2 メソッド一覧

| メソッド | 戻り値 | 説明 |
|---|---|---|
| `open(cfg)` | `bool` | デバイスをオープンしSPIパラメータを設定する |
| `close()` | `void` | デバイスをクローズする。未オープン時はno-op |
| `transfer(tx, rx, len)` | `int` | フルデュプレクス転送。転送バイト数を返す。エラー時は -1 |
| `is_open()` | `bool` | デバイスがオープン中なら true |
| `last_errno()` | `int` | 直近エラーの errno 値 |

---

## 2. SpiDriver クラス

Linux `spidev` キャラクタデバイスを介したSPI通信ドライバ（実機用）。
`ISpiDriver` を実装したクラスで、`/dev/spidevX.Y` の `open` / `ioctl` / `close` を管理する。

コピー禁止（RAII）。スレッドセーフではない。

### 2.1 コンストラクタ / デストラクタ

```cpp
explicit SpiDriver(const std::string& device_path);
~SpiDriver();  // open 中なら自動的に close する

SpiDriver(const SpiDriver&)            = delete;
SpiDriver& operator=(const SpiDriver&) = delete;
```

| パラメータ | 説明 |
|---|---|
| `device_path` | spidev のデバイスパス（例: `"/dev/spidev0.0"`）|

---

### 2.2 open()

```cpp
[[nodiscard]] bool open(const Config& cfg) noexcept;
```

**説明**: デバイスをオープンしSPIパラメータを設定する。

内部処理:
1. `::open(device_path, O_RDWR)` でファイルをオープン
2. `ioctl(SPI_IOC_WR_MODE)` でSPIモードを設定
3. `ioctl(SPI_IOC_WR_BITS_PER_WORD)` でビット幅を設定
4. `ioctl(SPI_IOC_WR_MAX_SPEED_HZ)` でクロック速度を設定
5. いずれかが失敗した場合 `fd` をクローズして `false` を返す

| 戻り値 | 条件 |
|---|---|
| `true` | オープン成功 |
| `false` | デバイスが存在しない、権限不足、ioctl 失敗 |

> `[[nodiscard]]` 指定のため、戻り値を無視するとコンパイル警告が出る。

---

### 2.3 close()

```cpp
void close() noexcept;
```

**説明**: デバイスをクローズする。未オープンの状態で呼んでも安全（no-op）。

デストラクタ内でも自動的に呼ばれる（RAII）。

---

### 2.4 transfer()

```cpp
[[nodiscard]] int transfer(const uint8_t* tx, uint8_t* rx, size_t len) noexcept;
```

**説明**: フルデュプレクスSPI転送を行う。`EAGAIN` 発生時は最大3回リトライする。

| パラメータ | 型 | 説明 |
|---|---|---|
| `tx` | `const uint8_t*` | 送信バッファ（len バイト）|
| `rx` | `uint8_t*` | 受信バッファ（len バイト）|
| `len` | `size_t` | 転送バイト数 |

| 戻り値 | 条件 |
|---|---|
| `>= 0` | 転送成功。転送バイト数を返す |
| `-1` | エラー（`last_errno()` で原因を確認）|

**リトライ仕様**:
- `EAGAIN` が返った場合のみリトライ（一時的なリソース不足）
- 最大3回試みて失敗した場合は `-1` を返す

> `[[nodiscard]]` 指定のため、戻り値を無視するとコンパイル警告が出る。

---

### 2.5 is_open()

```cpp
[[nodiscard]] bool is_open() const noexcept;
```

**説明**: デバイスがオープン中かどうかを返す。

| 戻り値 | 条件 |
|---|---|
| `true` | `open()` に成功し、まだ `close()` されていない |
| `false` | 未オープン、または `close()` 済み |

---

### 2.6 last_errno()

```cpp
[[nodiscard]] int last_errno() const noexcept;
```

**説明**: 直近のエラーで設定された `errno` 値を返す。
エラーが発生していない場合は `0` を返す。

---

## 3. 使用例

```cpp
#include <spi_driver.hpp>
#include <cstdio>

int main()
{
    embedded::SpiDriver drv("/dev/spidev0.0");

    embedded::ISpiDriver::Config cfg;
    cfg.speed_hz      = 1000000;  // 1 MHz
    cfg.bits_per_word = 8;
    cfg.mode          = 0;        // SPI_MODE_0

    if (!drv.open(cfg)) {
        fprintf(stderr, "open failed: errno=%d\n", drv.last_errno());
        return 1;
    }

    // MCP3008: スタートビット, シングルエンド CH0 選択, クロック供給用ダミー
    uint8_t tx[] = {0x01, 0x80, 0x00};
    uint8_t rx[3] = {};

    int n = drv.transfer(tx, rx, sizeof(tx));
    if (n < 0) {
        fprintf(stderr, "transfer failed: errno=%d\n", drv.last_errno());
        return 1;
    }

    uint16_t raw = ((rx[1] & 0x03) << 8) | rx[2];  // 下位 10bit を組み立てる
    printf("CH0 raw = %u (%d bytes transferred)\n", raw, n);
    return 0;
    // drv のデストラクタが自動的に close() を呼ぶ（RAII）
}
```

---

## 4. エラーハンドリング方針

- エラー発生時は `last_errno_` に `errno` を保存する
- **例外は使用しない**（組み込み環境を考慮した `noexcept` 設計）
- 呼び出し元は戻り値で成否を判断し、必要に応じて `last_errno()` を参照する

---

## 5. スレッド安全性

`SpiDriver` はスレッドセーフではない。複数スレッドから同一インスタンスを使用する場合は
呼び出し元でミューテックス管理を行うこと。

---

## 6. バージョン履歴

| バージョン | 変更内容 |
|---|---|
| v1.0.1 | `EAGAIN` 時に最大3回リトライする仕様を追加（CHG-002）|
| v1.0.0 | 初版。`open`, `close`, `transfer`, `is_open`, `last_errno` |
