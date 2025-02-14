# API仕様書 — libdevice

| 項目 | 内容 |
|---|---|
| ドキュメント番号 | API-LIB-001 |
| バージョン | 1.1 |
| ヘッダ | `#include <device.hpp>` |
| リンク | `-ldevice -lpthread` |
| 名前空間 | `embedded::` |

---

## 1. Device クラス

### コンストラクタ / デストラクタ

```cpp
explicit Device(const std::string& spi_path);
~Device();
```

| パラメータ | 説明 |
|---|---|
| `spi_path` | SPIデバイスパス（例: `/dev/spidev0.0`） |

---

### open()

```cpp
bool open();
```

**説明**: デバイスをオープンしSPIを初期化する。

| 戻り値 | 条件 |
|---|---|
| `true` | オープン成功 |
| `false` | デバイスが存在しない、権限不足など |

---

### close()

```cpp
void close();
```

**説明**: デバイスをクローズする。未オープンの状態で呼んでも安全（no-op）。

---

### read()

```cpp
std::vector<uint8_t> read(uint8_t reg, size_t len);
```

**説明**: 指定レジスタから `len` バイトを同期読み出しする。

| パラメータ | 説明 |
|---|---|
| `reg` | 読み出し元レジスタアドレス |
| `len` | 読み出しバイト数 |

| 戻り値 | 条件 |
|---|---|
| `vector<uint8_t>（len バイト）` | 成功 |
| 空の `vector` | 失敗（未オープン、転送エラー） |

---

### write()

```cpp
bool write(uint8_t reg, const std::vector<uint8_t>& data);
```

**説明**: 指定レジスタへ `data` を書き込む。

| パラメータ | 説明 |
|---|---|
| `reg` | 書き込み先レジスタアドレス |
| `data` | 書き込むバイト列 |

| 戻り値 | 条件 |
|---|---|
| `true` | 成功 |
| `false` | 失敗 |

---

### read_async() *(v1.1.0追加)*

```cpp
using ReadCallback = std::function<void(const std::vector<uint8_t>&, int)>;
void read_async(uint8_t reg, size_t len, ReadCallback cb);
```

**説明**: 別スレッドで非同期読み出しを実行する。完了時に `cb` を呼ぶ。

| パラメータ | 説明 |
|---|---|
| `reg` | 読み出し元レジスタアドレス |
| `len` | 読み出しバイト数 |
| `cb` | 完了コールバック。第1引数: 読み出しデータ、第2引数: errno（成功時0） |

> **注意**: `Device` オブジェクトのライフタイムはコールバック完了まで呼び出し元が保証すること。

---

## 2. 使用例

```cpp
#include <device.hpp>
#include <iostream>

int main() {
    embedded::Device dev("/dev/spidev0.0");
    if (!dev.open()) {
        std::cerr << "open failed\n";
        return 1;
    }

    // 同期読み出し
    auto data = dev.read(0x00, 4);
    for (auto b : data) printf("%02x ", b);

    // 非同期読み出し (v1.1.0)
    dev.read_async(0x00, 4, [](const std::vector<uint8_t>& d, int err) {
        if (!err)
            for (auto b : d) printf("%02x ", b);
    });

    return 0;
}
```

---

## 3. バージョン履歴

| バージョン | 変更内容 |
|---|---|
| v1.0.0 | 初版。`open`, `close`, `read`, `write` |
| v1.1.0 | `read_async()` を追加（CHG-003） |
