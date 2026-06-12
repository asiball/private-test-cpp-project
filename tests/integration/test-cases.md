# 結合テスト仕様書 — MCP3008 実機

| 項目 | 内容 |
|---|---|
| 対象 | Sensor（libsensor）+ SpiDriver（spi-hal）+ Linux spidev + MCP3008 |
| テストフレームワーク | Google Test |
| 実行コマンド | `./build/test-integration/test_mcp3008` |
| 実機 | Raspberry Pi 3B+ + MCP3008（SPI0 接続） |
| 治具 | Vref/Vdd=3.3V、CS=CE0、SCK/MOSI/MISO は SPI0 既定ピン |

実機がない環境（`/dev/spidev0.0` 不在）では `GTEST_SKIP()` で自動スキップ。

実装は `test_mcp3008.cpp` にあり、各テストケース ID は GTest の
`TEST_F(Suite, Name)` または `TEST(Suite, Name)` と 1:1 で対応します。

---

### IT-001 全チャネルの読み出しが成功し、10bit 範囲に収まる

| 項目 | 内容 |
|---|---|
| 前提 | MCP3008 接続済み、Vref=3.3V |
| 入力 | CH0〜CH7 を `read_raw()` |
| 期待 | 全チャネル成功、`*raw <= 1023` |
| 種別 | 正常系 |
| 実装 | `test_mcp3008.cpp:Mcp3008Test.AllChannelsReturnValidRange` |

### IT-002 `read_voltage()` が 0V〜Vref の範囲を返す

| 項目 | 内容 |
|---|---|
| 前提 | Vref=3.3V |
| 入力 | CH0〜CH7 を `read_voltage()` |
| 期待 | `0.0 ≤ v ≤ 3.3 + ε` |
| 種別 | 正常系 |
| 実装 | `test_mcp3008.cpp:Mcp3008Test.ReadVoltageWithinVref` |

### IT-003 100 回連続読み出しでエラー 0 件

| 項目 | 内容 |
|---|---|
| 前提 | MCP3008 接続済み |
| 入力 | CH0 を 100 回 `read_raw()` |
| 期待 | `nullopt` を返した回数 == 0 |
| 種別 | 安定性 |
| 実装 | `test_mcp3008.cpp:Mcp3008Test.Stability100ReadsOnCh0` |

### IT-004 `read_raw_async()` のコールバックが呼ばれる

| 項目 | 内容 |
|---|---|
| 前提 | MCP3008 接続済み |
| 入力 | `read_raw_async(0, cb)` |
| 期待 | 3 秒以内に cb 呼出、`err == 0`、`*raw <= 1023` |
| 種別 | 非同期 / 正常系 |
| 実装 | `test_mcp3008.cpp:Mcp3008Test.AsyncReadCallbackIsCalled` |

### IT-005 無効デバイスはオープン失敗

| 項目 | 内容 |
|---|---|
| 前提 | デバイス不在 |
| 入力 | `Sensor("/dev/spidevXX.0").open()` |
| 期待 | false、後続の `read_raw()` も `nullopt` |
| 種別 | 異常系 |
| 実装 | `test_mcp3008.cpp:Mcp3008Error.InvalidDeviceFailsToOpen` |

---

## libadxl345 / ADS1115 / gpio 結合テスト（実機必須）

`test_adxl345.cpp` / `test_ads1115.cpp` / `test_gpio.cpp` に対応。MCP3008 と同様、
対応する実機/デバイスノードが無い環境では各 `SetUp()` で `GTEST_SKIP()` する。

### IT-006 ADXL345 の DEVID が 0xE5

| 項目 | 内容 |
|---|---|
| 前提 | ADXL345 を SPI0 に接続 |
| 入力 | `read_device_id()` |
| 期待 | `0xE5`（疎通・誤配線検出）|
| 種別 | 正常系（実機のみ）|
| 実装 | `test_adxl345.cpp:Adxl345IntegrationTest.DeviceIdIsExpected` |

### IT-007 ADXL345 の read_g() が ±16g 内

| 項目 | 内容 |
|---|---|
| 前提 | ADXL345 接続済み |
| 入力 | `read_g()` |
| 期待 | x/y/z とも `|値| <= 16.5g` |
| 種別 | 正常系（実機のみ）|
| 実装 | `test_adxl345.cpp:Adxl345IntegrationTest.ReadGWithinRange` |

### IT-008 ADS1115 全チャネルがフルスケール内

| 項目 | 内容 |
|---|---|
| 前提 | ADS1115 を I2C1（0x48）に接続 |
| 入力 | 各 ch を `read_voltage()` |
| 期待 | `-0.1 <= V <= full_scale + 0.1` |
| 種別 | 正常系（実機のみ）|
| 実装 | `test_ads1115.cpp:Ads1115IntegrationTest.AllChannelsConvertWithinFullScale` |

### IT-009 ADS1115 の start_conversion → read_result

| 項目 | 内容 |
|---|---|
| 前提 | ADS1115 接続済み |
| 入力 | `start_conversion(0)` → 待機 → `read_result()` |
| 期待 | いずれも成功（割り込み駆動用 API の疎通）|
| 種別 | 正常系（実機のみ）|
| 実装 | `test_ads1115.cpp:Ads1115IntegrationTest.StartAndReadResult` |

### IT-010 GPIO エッジイベント要求が成功

| 項目 | 内容 |
|---|---|
| 前提 | `/dev/gpiochip0` が存在する実機 |
| 入力 | `request_edge_events(Both)` |
| 期待 | true、`is_requested()` true |
| 種別 | 正常系（実機のみ）|
| 実装 | `test_gpio.cpp:GpioIntegrationTest.RequestEdgeEventsSucceeds` |

### IT-011 GPIO wait_event がエッジ無しでタイムアウト

| 項目 | 内容 |
|---|---|
| 前提 | `/dev/gpiochip0` が存在し、信号源が無い |
| 入力 | `wait_event(100)` |
| 期待 | `0`（タイムアウト）|
| 種別 | 境界値（実機のみ）|
| 実装 | `test_gpio.cpp:GpioIntegrationTest.WaitEventTimesOutWithoutEdge` |
