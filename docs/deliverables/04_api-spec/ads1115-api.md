# API仕様書 — Ads1115

| 項目 | 内容 |
|---|---|
| ドキュメント番号 | API-ADS-001 |
| バージョン | 1.0 |
| 作成日 | 2026-06-01 |
| ヘッダ | `#include <ads1115.hpp>` |
| リンク | `-lads1115`（要 i2c-hal）|
| 名前空間 | `embedded::` |

> ADS1115（4ch / 16bit I2C ADC）の高レベルアクセスクラス。`Sensor`（MCP3008 = SPI の 10bit ADC）と「同じ ADC を別バス・別分解能で」行う対。`II2cDriver` に依存し **PIMPL** で実装を隠蔽する。
> レジスタ詳細は [ADS1115 レジスタマップ仕様書（IF-ADS-001）](../05_interface-spec/ads1115-register-map.md) を参照。

> **ビルド上の注意**: `ads1115` ターゲットは `i2c-hal` が存在する場合のみビルドされる任意ライブラリ。i2c-hal を外しても `sensor`（MCP3008/SPI）は影響を受けない。

---

## 1. 概要

- 4 チャネル（A0〜A3, シングルエンド）、16bit（符号付き）。
- 入力レンジは **PGA（ゲイン）**で可変（±6.144V〜±0.256V、デフォルト ±2.048V）。
- 読み出しは**シングルショット**: Config 書き込みで変換開始 → 完了待ち → Conversion 読み出し。

## 2. 定数 / 型

```cpp
static constexpr uint16_t DEFAULT_ADDR   = 0x48;  // ADDR ピン = GND
static constexpr uint8_t  CHANNEL_COUNT  = 4;

enum class Gain : uint8_t {
    FSR_6_144V = 0,  // ±6.144V
    FSR_4_096V = 1,  // ±4.096V
    FSR_2_048V = 2,  // ±2.048V（デフォルト）
    FSR_1_024V = 3,  // ±1.024V
    FSR_0_512V = 4,  // ±0.512V
    FSR_0_256V = 5,  // ±0.256V
};
```

## 3. コンストラクタ / デストラクタ

```cpp
explicit Ads1115(const std::string& i2c_path, uint16_t addr = DEFAULT_ADDR);  // 実機用
explicit Ads1115(II2cDriver* driver,           uint16_t addr = DEFAULT_ADDR);  // テスト用（DI）
~Ads1115();   // open 中なら自動 close（RAII）

Ads1115(const Ads1115&)            = delete;
Ads1115& operator=(const Ads1115&) = delete;
```

| パラメータ | 説明 |
|---|---|
| `i2c_path` | i2c-dev のデバイスパス（例: `"/dev/i2c-1"`）|
| `driver` | 既存の `II2cDriver` 実装を注入（`MockI2cDriver` 等）。所有権は渡さない |
| `addr` | スレーブアドレス（既定 `0x48`。ADDR ピンで `0x48`〜`0x4B`）|

## 4. メソッド

| メソッド | 戻り値 | 説明 |
|---|---|---|
| `open()` | `bool` | バスをオープンしスレーブアドレスを設定する |
| `close()` | `void` | バスをクローズする |
| `is_open()` | `bool` | オープン中なら true |
| `set_gain(g)` | `void` | ゲイン（入力レンジ）を設定する |
| `gain()` | `Gain` | 現在のゲイン |
| `full_scale_volts()` | `double` | 現在のゲインでのフルスケール電圧 [V] |
| `read_raw(channel)` | `optional<int16_t>` | 指定 ch をシングルショット変換し生値を読む。`channel` は 0〜3。失敗時 `nullopt` |
| `read_voltage(channel)` | `optional<double>` | 電圧 [V]（`raw * full_scale_volts() / 32768`）|
| `enable_conversion_ready_pin()` | `bool` | ALERT/RDY を変換完了通知として有効化（GPIO 割り込み連携用）|

> `read_raw()` は内部で Config の OS ビットをポーリングして変換完了を待つ。割り込み駆動にしたい場合は `enable_conversion_ready_pin()` + [GpioLine](gpio-api.md) を使う。

---

## 5. 使用例

```cpp
#include <ads1115.hpp>
#include <cstdio>

int main()
{
    embedded::Ads1115 adc("/dev/i2c-1");      // ADDR=GND → 0x48
    if (!adc.open()) return 1;
    adc.set_gain(embedded::Ads1115::Gain::FSR_4_096V);

    if (auto v = adc.read_voltage(0)) {
        printf("ch0 = %.4f V\n", *v);
    }
    return 0;
}
```

割り込み駆動（ALERT/RDY + GPIO）の例は [GPIO API（API-GPIO-001）](gpio-api.md) §4.2 を参照。

## 6. エラーハンドリング / スレッド安全性

- 失敗は `std::optional`（`nullopt`）/ `bool`（`false`）で表し、例外は使用しない。
- 無効チャネル（`>= 4`）/ 未オープン / 転送失敗は `read_raw` → `nullopt`。
- スレッドセーフではない。

## 7. バージョン履歴

| バージョン | 変更内容 |
|---|---|
| 1.0 | 初版。`open`/`close`、`set_gain`/`gain`/`full_scale_volts`、`read_raw`/`read_voltage`、`enable_conversion_ready_pin` |
