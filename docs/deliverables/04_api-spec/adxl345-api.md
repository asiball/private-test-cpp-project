# API仕様書 — Adxl345

| 項目 | 内容 |
|---|---|
| ドキュメント番号 | API-ADXL-001 |
| バージョン | 1.0 |
| 作成日 | 2026-06-01 |
| ヘッダ | `#include <adxl345.hpp>` |
| リンク | `-ladxl345` |
| 名前空間 | `embedded::` |

> ADXL345（3軸 SPI 加速度センサ。最大 13bit、16bit レジスタ格納）の高レベルアクセスクラス。
> `SpiDriver` を **PIMPL** で隠蔽し、レジスタアクセス層（`read_reg`/`write_reg`/`update_bits`）の上に加速度読み出し API を提供する。
> レジスタの詳細は [ADXL345 レジスタマップ仕様書（IF-ADXL-001）](../05_interface-spec/adxl345-register-map.md) を参照。

---

## 1. 概要

- SPI モード **MODE 3**（CPOL=1, CPHA=1）。本実装は 1 MHz でオープン（ADXL345 上限 5 MHz に対し安全側）。
- `open()` 内で **DEVID 確認 → 既定設定（フル分解能 + ±16g）→ 測定開始**まで行う。
- MCP3008（レジスタ無しのコマンド型 ADC）と対照的に、ADXL345 はレジスタマップを持ち設定をビット単位で行う。

## 2. 定数 / 型

```cpp
static constexpr double SCALE_G_PER_LSB = 0.0039;   // フル分解能 3.9 mg/LSB
struct AccelRaw { int16_t x; int16_t y; int16_t z; };   // 3軸 生値（符号付き16bit）
struct AccelG   { double  x; double  y; double  z; };   // 3軸 加速度 [g]
```

## 3. コンストラクタ / デストラクタ

```cpp
explicit Adxl345(const std::string& spi_path);  // 実機用
explicit Adxl345(ISpiDriver* driver);           // テスト用（DI、所有権は渡さない）
~Adxl345();                                      // open 中なら自動 close（RAII）

Adxl345(const Adxl345&)            = delete;
Adxl345& operator=(const Adxl345&) = delete;
```

| パラメータ | 説明 |
|---|---|
| `spi_path` | spidev のデバイスパス（例: `"/dev/spidev0.0"`）|
| `driver` | 既存の `ISpiDriver` 実装を注入（`MockSpiDriver` 等での実機不要テスト用）|

## 4. メソッド

### 4.1 ライフサイクル

| メソッド | 戻り値 | 説明 |
|---|---|---|
| `open()` | `bool` | SPI を MODE 3 でオープン → DEVID(0x00)==0xE5 確認 → DATA_FORMAT に FULL_RES\|±16g → POWER_CTL の MEASURE を立てる。いずれか失敗で close し false |
| `close()` | `void` | デバイスをクローズする |
| `is_open()` | `bool` | オープン中なら true |

### 4.2 レジスタアクセス層

| メソッド | 戻り値 | 説明 |
|---|---|---|
| `read_reg(addr)` | `optional<uint8_t>` | 1 バイトレジスタを読む。失敗時 `nullopt` |
| `write_reg(addr, value)` | `bool` | 1 バイトレジスタへ書く |
| `update_bits(addr, mask, value)` | `bool` | `mask` のビットだけ read-modify-write で更新 |

### 4.3 高レベル API

| メソッド | 戻り値 | 説明 |
|---|---|---|
| `read_device_id()` | `optional<uint8_t>` | DEVID(0x00)。正常時 `0xE5` |
| `read_raw()` | `optional<AccelRaw>` | 3 軸の生値をマルチバイト一括読み出し |
| `read_g()` | `optional<AccelG>` | 3 軸を [g] で読む（各軸 × `SCALE_G_PER_LSB`）|

---

## 5. 使用例

```cpp
#include <adxl345.hpp>
#include <cstdio>

int main()
{
    embedded::Adxl345 dev("/dev/spidev0.0");
    if (!dev.open()) {          // open 内で DEVID 確認 + 設定 + 測定開始
        fprintf(stderr, "open failed (DEVID 不一致 or 転送失敗)\n");
        return 1;
    }
    if (auto a = dev.read_g()) {
        printf("x=%.3f y=%.3f z=%.3f [g]\n", a->x, a->y, a->z);
    }
    return 0;
}
```

## 6. エラーハンドリング / スレッド安全性

- 失敗は `std::optional`（`nullopt`）/ `bool`（`false`）で表し、例外は使用しない。
- スレッドセーフではない。複数スレッドから使う場合は呼び出し側で排他制御する。

## 7. バージョン履歴

| バージョン | 変更内容 |
|---|---|
| 1.0 | 初版。レジスタアクセス層 + `read_device_id` / `read_raw` / `read_g` |
