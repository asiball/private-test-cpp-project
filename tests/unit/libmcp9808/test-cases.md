# 単体テスト仕様書 — libmcp9808

| 項目 | 内容 |
|---|---|
| 対象モジュール | `libmcp9808/` (libmcp9808.so) |
| 対象クラス | `Mcp9808`（MCP9808 / I2C 温度センサ） |
| テストフレームワーク | Google Test + GMock |
| 実行コマンド | `./build/test-libmcp9808/test_mcp9808` |
| カバレッジ目標 | 主要メソッド 80% 以上 |

`II2cDriver` のモック（`MockI2cDriver`）を注入し、実機 I2C なしで検証する。
各テストケース ID は GTest の `TEST(Suite, Name)` と 1:1 で対応する。

---

### UT-MCP-001 無効パスで `open()` 失敗

| 項目 | 内容 |
|---|---|
| 前提 | デバイス不在（実機ドライバ経由）|
| 入力 | `/dev/i2cXX` を開く |
| 期待 | `open()` false、`is_open()` false |
| 種別 | 異常系 |
| 実装 | `test_mcp9808.cpp:Mcp9808Open.InvalidDeviceReturnsFalse` |

### UT-MCP-002 `open()` が製造者 ID を確認して成功

| 項目 | 内容 |
|---|---|
| 前提 | Mock が MANUF_ID(0x06) に 0x0054 を返す |
| 入力 | `open()` |
| 期待 | true |
| 種別 | 正常系 |
| 実装 | `test_mcp9808.cpp:Mcp9808Open.VerifiesManufacturerId` |

### UT-MCP-003 製造者 ID 不一致なら `open()` 失敗

| 項目 | 内容 |
|---|---|
| 前提 | Mock が誤った ID を返す |
| 入力 | `open()` |
| 期待 | false（close が呼ばれる）|
| 種別 | 異常系 |
| 実装 | `test_mcp9808.cpp:Mcp9808Open.WrongManufacturerIdFailsOpen` |

### UT-MCP-004 正の温度を 0.0625°C 分解能で換算

| 項目 | 内容 |
|---|---|
| 前提 | Mock が T_AMBIENT(0x05) に 0x0194 を返す |
| 入力 | `read_temperature()` |
| 期待 | `+25.25 °C`（誤差 1e-9）|
| 種別 | 正常系 |
| 実装 | `test_mcp9808.cpp:Mcp9808Temp.ReadsPositiveTemperature` |

### UT-MCP-005 フラグビットを無視して温度部のみ解釈

| 項目 | 内容 |
|---|---|
| 前提 | Mock が 0xE194（上位 3bit フラグ立て）を返す |
| 入力 | `read_temperature()` |
| 期待 | `+25.25 °C`（フラグ 15:13 を除去）|
| 種別 | 境界値 |
| 実装 | `test_mcp9808.cpp:Mcp9808Temp.IgnoresFlagBits` |

### UT-MCP-006 負の温度を符号ビットで換算

| 項目 | 内容 |
|---|---|
| 前提 | Mock が 0x1FF0（符号ビット立て）を返す |
| 入力 | `read_temperature()` |
| 期待 | `-1.0 °C`（誤差 1e-9）|
| 種別 | 正常系 |
| 実装 | `test_mcp9808.cpp:Mcp9808Temp.ReadsNegativeTemperature` |

### UT-MCP-007 転送失敗で `std::nullopt`

| 項目 | 内容 |
|---|---|
| 前提 | Mock の write_read が -1 を返す |
| 入力 | `read_temperature()` |
| 期待 | `std::nullopt` |
| 種別 | 異常系 |
| 実装 | `test_mcp9808.cpp:Mcp9808Temp.TransferErrorReturnsNullopt` |

### UT-MCP-008 コピー禁止確認

| 項目 | 内容 |
|---|---|
| 前提 | — |
| 入力 | 型特性 |
| 期待 | コピー構築 / 代入とも false |
| 種別 | 正常系 |
| 実装 | `test_mcp9808.cpp:Mcp9808Copyable.IsNotCopyConstructible` |
