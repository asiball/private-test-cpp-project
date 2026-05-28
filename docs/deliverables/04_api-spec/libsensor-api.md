# API仕様書 — libsensor

| 項目 | 内容 |
|---|---|
| ドキュメント番号 | API-LIB-001 |
| バージョン | 1.1 |
| ヘッダ | `#include <sensor.hpp>` |
| リンク | `-lsensor -lpthread` |
| 名前空間 | `embedded::` |

> `Sensor` クラスは MCP3008（8ch / 10bit SPI ADC）からアナログ値を読み出すための
> 高レベル API を提供する。低レベルの SPI 転送は `spi-hal`（`ISpiDriver`）に委譲する。

---

## 1. 定数

```cpp
static constexpr uint8_t  CHANNEL_COUNT = 8;     // MCP3008 のチャネル数
static constexpr uint16_t ADC_MAX       = 1023;  // 10bit ADC のフルスケール値
static constexpr double   DEFAULT_VREF  = 3.3;   // 既定の基準電圧 [V]
```

---

## 2. Sensor クラス

### コンストラクタ / デストラクタ

```cpp
explicit Sensor(const std::string& spi_path, double vref = DEFAULT_VREF);
explicit Sensor(ISpiDriver* driver,          double vref = DEFAULT_VREF);  // DI（テスト用）
~Sensor();
```

| パラメータ | 説明 |
|---|---|
| `spi_path` | SPIデバイスパス（例: `/dev/spidev0.0`） |
| `driver` | 既存の `ISpiDriver` 実装を注入（モックによる実機不要テスト用）。所有権は移動しない |
| `vref` | 基準電圧 [V]。電圧換算（`read_voltage`）に使用。省略時 `DEFAULT_VREF`（3.3V） |

> コピー構築・コピー代入は禁止（`= delete`）。

---

### open() / close() / is_open()

```cpp
[[nodiscard]] bool open() noexcept;
void               close() noexcept;
[[nodiscard]] bool is_open() const noexcept;
```

**説明**: デバイスをオープン／クローズする。`close()` は未オープンでも安全（no-op）。
デストラクタはオープン中なら自動的に `close()` を呼ぶ（RAII）。

| `open()` 戻り値 | 条件 |
|---|---|
| `true` | オープン成功 |
| `false` | デバイスが存在しない、権限不足など |

---

### read_raw()

```cpp
[[nodiscard]] std::optional<uint16_t> read_raw(uint8_t channel);
```

**説明**: 指定チャネルの 10bit raw 値（0〜`ADC_MAX`）を同期読み出しする。

| パラメータ | 説明 |
|---|---|
| `channel` | チャネル番号（0〜7）。範囲外は失敗 |

| 戻り値 | 条件 |
|---|---|
| `uint16_t`（0〜1023） | 成功 |
| `std::nullopt` | 失敗（未オープン、範囲外チャネル、転送エラー） |

---

### read_voltage()

```cpp
[[nodiscard]] std::optional<double> read_voltage(uint8_t channel);
```

**説明**: 指定チャネルを読み出し、電圧 [V] に換算して返す。
内部で `read_raw(channel) * vref() / ADC_MAX` を計算する。

| 戻り値 | 条件 |
|---|---|
| `double`（電圧 [V]） | 成功 |
| `std::nullopt` | 失敗（`read_raw` と同条件） |

---

### read_raw_async() *(v1.1.0追加)*

```cpp
using ReadCallback = std::function<void(std::optional<uint16_t>, int)>;
void read_raw_async(uint8_t channel, ReadCallback cb);
```

**説明**: 別スレッドで非同期読み出しを実行する。完了時に `cb` を呼ぶ。

| パラメータ | 説明 |
|---|---|
| `channel` | チャネル番号（0〜7） |
| `cb` | 完了コールバック。第1引数: raw 値（失敗時 `std::nullopt`）、第2引数: errno（成功時0） |

> **注意**: `Sensor` オブジェクトのライフタイムはコールバック完了まで呼び出し元が保証すること。

---

### vref() / set_vref()

```cpp
[[nodiscard]] double vref() const noexcept;
void                 set_vref(double vref) noexcept;
```

**説明**: 電圧換算に使う基準電圧 [V] の取得・変更。

---

## 3. 使用例

```cpp
#include <sensor.hpp>
#include <iostream>

int main() {
    embedded::Sensor sensor("/dev/spidev0.0");  // Vref=3.3V
    if (!sensor.open()) {
        std::cerr << "open failed\n";
        return 1;
    }

    // 同期読み出し（CH0）
    if (auto raw = sensor.read_raw(0)) {
        std::cout << "CH0 raw = " << *raw << '\n';
    }
    if (auto v = sensor.read_voltage(0)) {
        std::cout << "CH0 voltage = " << *v << " V\n";
    }

    // 非同期読み出し (v1.1.0)
    sensor.read_raw_async(0, [](std::optional<uint16_t> raw, int err) {
        if (raw) std::cout << "async CH0 raw = " << *raw << '\n';
        else     std::cerr << "async read error: " << err << '\n';
    });

    return 0;
}
```

---

## 4. バージョン履歴

| バージョン | 変更内容 |
|---|---|
| v1.0.0 | 初版。`open`, `close`, `read_raw`, `read_voltage` |
| v1.1.0 | `read_raw_async()` を追加（CHG-003） |
