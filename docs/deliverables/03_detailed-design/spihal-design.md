# 詳細設計書 — SpiDriver

| 項目 | 内容 |
|---|---|
| ドキュメント番号 | DES-DRV-001 |
| バージョン | 1.1 |
| 作成日 | 2025-03-07 |
| 作成者 | 山田 太郎 |

---

## 1. クラス概要

`SpiDriver` は `/dev/spidev` キャラクタデバイスを介してSPIデバイスとフルデュプレクス通信を行う。ファイルディスクリプタのライフタイムをクラスで管理し、コピー禁止とする。

## 2. クラス図

```mermaid
classDiagram
    class SpiDriver {
        -device_path_ std::string
        -fd_ int
        -last_errno_ int
        +SpiDriver(device_path)
        +~SpiDriver()
        +open(cfg) bool
        +close() void
        +transfer(tx, rx, len) int
        +is_open() bool
        +last_errno() int
    }
    class Config {
        <<struct>>
        +speed_hz uint32_t
        +bits_per_word uint8_t
        +mode uint8_t
    }
    SpiDriver ..> Config : open() で受け取る
```

## 3. メソッド詳細

### 3.1 open()

```
入力: Config（speed_hz, bits_per_word, mode）
処理:
  1. ::open(device_path_, O_RDWR) → fd_
  2. ioctl SPI_IOC_WR_MODE
  3. ioctl SPI_IOC_WR_BITS_PER_WORD
  4. ioctl SPI_IOC_WR_MAX_SPEED_HZ
  いずれかが失敗した場合 fd_ をクローズして false を返す
出力: bool（成功/失敗）
```

### 3.2 transfer() — 引数検証・リトライシーケンス

```mermaid
flowchart TD
    A["transfer(tx, rx, len)"] --> V["validate_spi_transfer(fd, tx, rx, len)"]
    V --> V1{"fd < 0？"}
    V1 -- Yes --> ERR_BADF["return -1（EBADF）"]
    V1 -- No --> V2{"tx または rx が null？"}
    V2 -- Yes --> ERR_INVAL["return -1（EINVAL）"]
    V2 -- No --> V3{"len が uint32_t を超過？"}
    V3 -- Yes --> ERR_OVERFLOW["return -1（EOVERFLOW）"]
    V3 -- No --> V4{"len == 0？"}
    V4 -- Yes --> RET0["return 0"]
    V4 -- No --> B["spi_ioc_transfer 構造体を設定"]
    B --> C["retry = 0"]
    C --> D["ioctl(SPI_IOC_MESSAGE(1))"]
    D --> E{"成功？"}
    E -- Yes --> F["return n（転送バイト数）"]
    E -- No --> G{"errno == EAGAIN？"}
    G -- No --> H["return -1"]
    G -- Yes --> I{"retry < 2？"}
    I -- No --> H
    I -- Yes --> K["100µs << retry 待機（1 回目 100µs / 2 回目 200µs）"]
    K --> J["retry++"]
    J --> D
```

`retry` は 0 始まりで、`ioctl(SPI_IOC_MESSAGE(1))` は最大 3 回（初回 + `EAGAIN` 時の再試行 2 回）実行される。
再試行前には指数バックオフ（1 回目は 100µs、2 回目は 200µs）を挟む
（詳細は [SpiDriver API 仕様書](../04_api-spec/spi-driver-api.md) 参照）。
引数検証（`validate_spi_transfer`）は `SpiDriver` と `KernelSpiDriver`（[カーネルドライバ設計書](kernel-driver-design.md)）で
共通の実装（`spi_transfer_validate.hpp`）を使う。

## 4. エラーハンドリング方針

- エラー発生時は `last_errno_` に `errno` を保存する
- 例外は使用しない（組み込み環境を考慮）
- 呼び出し元は戻り値で成否を判断し、必要に応じて `last_errno()` を参照する

## 5. スレッド安全性

`SpiDriver` はスレッドセーフではない。複数スレッドから同一インスタンスを使用する場合は、呼び出し元でミューテックス管理を行うこと。
