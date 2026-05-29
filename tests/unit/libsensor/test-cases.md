# 単体テスト仕様書 — libsensor

| 項目 | 内容 |
|---|---|
| 対象モジュール | `libsensor/` (libsensor.so) |
| 対象クラス | `Sensor`（MCP3008 制御） |
| テストフレームワーク | Google Test + GMock |
| 実行コマンド | `./build/test-libsensor/test_sensor` |
| カバレッジ目標 | 主要メソッド 80% 以上 |

実装は `test_sensor.cpp` にあり、各テストケース ID は GTest の
`TEST(Suite, Name)` と 1:1 で対応します。Mock を使った実機不要の
ケースが中心です。

---

### UT-LIB-001 有効パスで `open()` 成功

| 項目 | 内容 |
|---|---|
| 前提 | `/dev/spidev0.0` が存在する実機環境 |
| 入力 | 有効な spidev パス |
| 期待 | `open()` true、`is_open()` true |
| 種別 | 正常系（実機のみ） |
| 実装 | `test_sensor.cpp:SensorOpen.ValidDeviceReturnsTrue` |

### UT-LIB-002 無効パスで `open()` 失敗

| 項目 | 内容 |
|---|---|
| 前提 | デバイス不在 |
| 入力 | `/dev/spidevXX.0` |
| 期待 | `open()` false、`is_open()` false |
| 種別 | 異常系 |
| 実装 | `test_sensor.cpp:SensorOpen.InvalidDeviceReturnsFalse` |

### UT-LIB-003 `read_raw()` が 10bit 値を返す（Mock）

| 項目 | 内容 |
|---|---|
| 前提 | MockSpiDriver で transfer が 0x2A5（677）を返すよう設定 |
| 入力 | `read_raw(0)` |
| 期待 | `*raw == 0x2A5`（MCP3008 プロトコルで `(rx[1]&0x03)<<8 \| rx[2]`）|
| 種別 | 正常系 |
| 実装 | `test_sensor.cpp:SensorReadRaw.ReturnsTenBitValueViaMock` |

### UT-LIB-004 範囲外チャネルは `std::nullopt`

| 項目 | 内容 |
|---|---|
| 前提 | — |
| 入力 | `read_raw(8)`, `read_raw(255)` |
| 期待 | `std::nullopt`（transfer は呼ばれない） |
| 種別 | 境界値 |
| 実装 | `test_sensor.cpp:SensorReadRaw.InvalidChannelReturnsNullopt` |

### UT-LIB-005 `read_voltage()` が Vref に応じて変換される

| 項目 | 内容 |
|---|---|
| 前提 | Vref=5.0V、Mock が raw=512 を返す |
| 入力 | `read_voltage(0)` |
| 期待 | `512 * 5.0 / 1023 ≒ 2.502 V`（誤差 1e-9） |
| 種別 | 正常系 |
| 実装 | `test_sensor.cpp:SensorReadVoltage.ScalesByVref` |

### UT-LIB-006 `set_vref()` で電圧計算式が切り替わる

| 項目 | 内容 |
|---|---|
| 前提 | Mock が raw=1023 を返す |
| 入力 | `set_vref(5.0)` 前後の `read_voltage(0)` |
| 期待 | 前 ≒ 3.3V、後 ≒ 5.0V |
| 種別 | 正常系 |
| 実装 | `test_sensor.cpp:SensorReadVoltage.SetVrefAffectsConversion` |

### UT-LIB-007 未オープンで `read_raw_async()` するとエラー通知

| 項目 | 内容 |
|---|---|
| 前提 | デバイス不在 |
| 入力 | `read_raw_async(0, cb)` |
| 期待 | コールバックが呼ばれ、`raw == nullopt` かつ `err != 0` |
| 種別 | 異常系 |
| 実装 | `test_sensor.cpp:SensorReadRawAsync.NotOpenCallbackReceivesError` |

### UT-LIB-008 MCP3008 TX 列がプロトコル仕様どおり

| 項目 | 内容 |
|---|---|
| 前提 | Mock の transfer 引数をキャプチャ |
| 入力 | `read_raw(3)` |
| 期待 | TX = `{0x01, 0x80\|(3<<4), 0x00}` = `{0x01, 0xB0, 0x00}` |
| 種別 | プロトコル確認 |
| 実装 | `test_sensor.cpp:SensorReadRaw.SendsCorrectMcp3008Command` |

### UT-LIB-009 コピー禁止確認

| 項目 | 内容 |
|---|---|
| 前提 | — |
| 入力 | 型特性 |
| 期待 | コピー構築 / 代入とも false |
| 種別 | 正常系 |
| 実装 | `test_sensor.cpp:SensorCopyable.IsNotCopyConstructible` |
