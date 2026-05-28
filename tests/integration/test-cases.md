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
