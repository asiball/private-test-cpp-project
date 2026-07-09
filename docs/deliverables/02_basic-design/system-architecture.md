# 基本設計書 — システム構成

| 項目 | 内容 |
|---|---|
| ドキュメント番号 | DES-001 |
| バージョン | 1.1 |
| 作成日 | 2025-02-14 |
| 作成者 | 山田 太郎 |

---

## 1. システム全体構成

```mermaid
flowchart TD
    subgraph user["ユーザ空間 (User Space)"]
        CLI["device-ctl<br/>(CLIツール)"]
        APP["発注者アプリケーション<br/>(将来的な利用者)"]
        LIB["libsensor.so<br/>Sensor クラス<br/>・read_raw() / read_voltage()<br/>・read_raw_async()"]
        HAL["libspihal.a<br/>SpiDriver クラス<br/>・open() / close()<br/>・transfer()"]
        CLI -->|C++ API| LIB
        APP -->|C++ API| LIB
        LIB -->|C++ API| HAL
    end
    subgraph kernel["Linux Kernel (kernel 5.10)"]
        SPIDEV["spidev カーネルドライバ<br/>/dev/spidev0.0"]
    end
    DEV["SPIデバイス<br/>(センサ等)"]
    HAL -->|"ioctl (SPI_IOC_MESSAGE)"| SPIDEV
    SPIDEV -->|"SPI (物理バス)"| DEV
```

> 図の Draw.io 版: [system-architecture.drawio](system-architecture.drawio)

## 2. コンポーネント間依存関係

```mermaid
graph TD
    CLI[device-ctl] --> LIB[libsensor.so]
    LIB --> DRV[libspihal.a]
    DRV --> SPIDEV["/dev/spidev0.0"]
    APP[発注者App] --> LIB
```

## 3. ビルド成果物

| 成果物 | 種別 | 提供先 |
|---|---|---|
| `libspihal.a` | 静的ライブラリ | libsensor内部リンク |
| `libsensor.so.1.1.0` | 動的共有ライブラリ | 発注者アプリ・CLI |
| `device-ctl` | 実行ファイル | 動作確認・デバッグ用 |

## 4. 実行環境

| 項目 | 仕様 |
|---|---|
| ハードウェア | Raspberry Pi 3B+ |
| OS | Linux kernel 5.10.x |
| コンパイラ | GCC 13 / Ubuntu 24.04（C++17） |
| SPIデバイス | /dev/spidev0.0（最大 2MHz） |

## 5. オプション拡張コンポーネント

§1〜§4 は**必須経路（SPI / MCP3008）**を示す。以降に追加された下表のコンポーネントは
**任意（★）**で、トップ CMake が「存在するものだけ」をビルドするため、ディレクトリごと
削除しても必須経路は影響を受けない（独立性の設計意図は [ADR 0001](../../adr/0001-optional-independent-components.md)）。
各コンポーネントの詳細設計・API・IF 仕様は [納品ドキュメント索引 §2](../README.md) を参照。

| コンポーネント | バス | 役割 | 成果物 |
|---|---|---|---|
| `i2c-hal` | I2C | I2C 通信の HAL | `libi2chal.a` |
| `libsensor`（Ads1115） | I2C | ADS1115（16bit ADC） | `libads1115.so` |
| `libadxl345` | SPI | ADXL345（レジスタ型加速度センサ） | `libadxl345.so` |
| `libmcp9808` | I2C | MCP9808（16bit 温度センサ） | `libmcp9808.so` |
| `gpio` | — | GPIO エッジ割り込み（epoll） | `libgpio.a` |
| `kernel` | SPI | 独自 SPI カーネルドライバ（学習用拡張） | `my_spi_driver.ko` |

> これらは必須経路と同じ設計原則（インターフェース分離 + DI + PIMPL、コンポーネント別バージョンタグ）に
> 従う。`libadxl345` は `spi-hal` を、`libsensor`(Ads1115) は `i2c-hal` を再利用し、`ISpiDriver` /
> `II2cDriver` という同型の抽象で実機/モックを差し替えられる。
