# 単体テスト仕様書 — libadxl345

| 項目 | 内容 |
|---|---|
| 対象モジュール | `libadxl345/` (libadxl345.so) |
| 対象クラス | `Adxl345`（ADXL345 制御 / レジスタアクセス層） |
| テストフレームワーク | Google Test + GMock |
| 実行コマンド | `./build/test-libadxl345/test_adxl345` |
| カバレッジ目標 | 主要メソッド 80% 以上 |

実装は `test_adxl345.cpp` にあり、各テストケース ID は GTest の
`TEST(Suite, Name)` と 1:1 で対応します。レジスタ表は
[adxl345-register-map.md](../../../docs/deliverables/05_interface-spec/adxl345-register-map.md) を参照。

---

### UT-ADXL-001 有効パスで `open()` がクラッシュしない

| 項目 | 内容 |
|---|---|
| 前提 | `/dev/spidev0.0` が存在する実機環境 |
| 入力 | 有効な spidev パス |
| 期待 | `open()` がクラッシュせず戻る（実機が ADXL345 か不問） |
| 種別 | 正常系（実機のみ） |
| 実装 | `test_adxl345.cpp:Adxl345Open.ValidDeviceReturnsTrue` |

### UT-ADXL-002 無効パスで `open()` 失敗

| 項目 | 内容 |
|---|---|
| 前提 | デバイス不在 |
| 入力 | `/dev/spidevXX.0` |
| 期待 | `open()` false、`is_open()` false |
| 種別 | 異常系 |
| 実装 | `test_adxl345.cpp:Adxl345Open.InvalidDeviceReturnsFalse` |

### UT-ADXL-003 `read_reg()` が READ フレーム付きアドレスを送る

| 項目 | 内容 |
|---|---|
| 前提 | Mock が rx[1]=0xE5 を返す |
| 入力 | `read_reg(DEVID)` |
| 期待 | TX[0] = `0x80\|0x00` = 0x80、戻り値 0xE5 |
| 種別 | プロトコル確認 |
| 実装 | `test_adxl345.cpp:Adxl345Reg.ReadRegSendsReadFramedAddress` |

### UT-ADXL-004 `write_reg()` が WRITE フレームで値を送る

| 項目 | 内容 |
|---|---|
| 前提 | Mock の transfer 引数をキャプチャ |
| 入力 | `write_reg(DATA_FORMAT, FULL_RES\|RANGE_16G)` |
| 期待 | TX = `{0x31, 0x0B}`（bit7=0 の書き込み） |
| 種別 | プロトコル確認 |
| 実装 | `test_adxl345.cpp:Adxl345Reg.WriteRegSendsWriteFramedAddressAndValue` |

### UT-ADXL-005 `update_bits()` が read-modify-write で他ビットを保持

| 項目 | 内容 |
|---|---|
| 前提 | Mock が現在値 0xA0 を返す |
| 入力 | `update_bits(DATA_FORMAT, RANGE_MASK, RANGE_16G)` |
| 期待 | 書き込み値 0xA3（上位 0xA0 を保持し下位 2bit のみ更新） |
| 種別 | 正常系 |
| 実装 | `test_adxl345.cpp:Adxl345Reg.UpdateBitsPreservesOtherBits` |

### UT-ADXL-006 `read_raw()` がリトルエンディアン符号付きで合成

| 項目 | 内容 |
|---|---|
| 前提 | Mock が X=+256, Y=-1, Z=+32767 のバイト列を返す |
| 入力 | `read_raw()` |
| 期待 | TX[0]=0xF2（READ\|MB\|DATAX0）、`x==256, y==-1, z==32767` |
| 種別 | プロトコル確認 |
| 実装 | `test_adxl345.cpp:Adxl345Data.ReadRawAssemblesLittleEndianSignedAxes` |

### UT-ADXL-007 `read_g()` が 3.9mg/LSB でスケール

| 項目 | 内容 |
|---|---|
| 前提 | Mock が X=256 LSB を返す |
| 入力 | `read_g()` |
| 期待 | `x ≒ 256 * 0.0039`（誤差 1e-9） |
| 種別 | 正常系 |
| 実装 | `test_adxl345.cpp:Adxl345Data.ReadGScalesByFullResFactor` |

### UT-ADXL-008 `open()` が DEVID 確認後に設定レジスタを書く

| 項目 | 内容 |
|---|---|
| 前提 | Mock が DEVID=0xE5 を返し、書き込みを記録 |
| 入力 | `open()` |
| 期待 | DATA_FORMAT←0x0B、POWER_CTL←MEASURE の順で書き込み |
| 種別 | シーケンス確認 |
| 実装 | `test_adxl345.cpp:Adxl345Open.ConfiguresDeviceWhenDevidMatches` |

### UT-ADXL-009 DEVID 不一致なら `open()` 失敗

| 項目 | 内容 |
|---|---|
| 前提 | Mock が DEVID=0x00 を返す |
| 入力 | `open()` |
| 期待 | false（誤デバイス検出）、`close()` が呼ばれる |
| 種別 | 異常系 |
| 実装 | `test_adxl345.cpp:Adxl345Open.WrongDevidFailsOpen` |

### UT-ADXL-010 転送失敗時は `read_raw()` が `std::nullopt`

| 項目 | 内容 |
|---|---|
| 前提 | Mock の transfer が -1 を返す |
| 入力 | `read_raw()` |
| 期待 | `std::nullopt` |
| 種別 | 異常系 |
| 実装 | `test_adxl345.cpp:Adxl345Data.TransferErrorReturnsNullopt` |

### UT-ADXL-011 コピー禁止確認

| 項目 | 内容 |
|---|---|
| 前提 | — |
| 入力 | 型特性 |
| 期待 | コピー構築 / 代入とも false |
| 種別 | 正常系 |
| 実装 | `test_adxl345.cpp:Adxl345Copyable.IsNotCopyConstructible` |

### UT-ADXL-012 `enable_tap_detection()` のレジスタ設定列

| 項目 | 内容 |
|---|---|
| 前提 | Mock の write を記録、read は 0 を返す |
| 入力 | `enable_tap_detection(0x20, 0x10, TAP_AXIS_Z)` |
| 期待 | THRESH_TAP / DUR / TAP_AXES / INT_MAP(=0) / INT_ENABLE(=SINGLE_TAP) の 5 書き込み |
| 種別 | 割り込み設定 |
| 実装 | `test_adxl345.cpp:Adxl345Interrupt.EnableTapDetectionWritesExpectedRegisters` |

### UT-ADXL-013 `enable_free_fall()` のレジスタ設定列

| 項目 | 内容 |
|---|---|
| 前提 | Mock の write を記録、read は 0 を返す |
| 入力 | `enable_free_fall(0x07, 0x28)` |
| 期待 | THRESH_FF / TIME_FF / INT_MAP(=0) / INT_ENABLE(=FREE_FALL) の 4 書き込み |
| 種別 | 割り込み設定 |
| 実装 | `test_adxl345.cpp:Adxl345Interrupt.EnableFreeFallWritesExpectedRegisters` |

### UT-ADXL-014 `disable_interrupts()` が INT_ENABLE=0

| 項目 | 内容 |
|---|---|
| 前提 | — |
| 入力 | `disable_interrupts()` |
| 期待 | `INT_ENABLE` に 0x00 を書く |
| 種別 | 割り込み設定 |
| 実装 | `test_adxl345.cpp:Adxl345Interrupt.DisableInterruptsClearsIntEnable` |

### UT-ADXL-015 `read_interrupt_source()` が INT_SOURCE を読む

| 項目 | 内容 |
|---|---|
| 前提 | Mock が INT_SOURCE 読みで SINGLE_TAP を返す |
| 入力 | `read_interrupt_source()` |
| 期待 | READ フレーム付き INT_SOURCE を送り、`INT_SINGLE_TAP` ビットが立つ |
| 種別 | 割り込み読み出し |
| 実装 | `test_adxl345.cpp:Adxl345Interrupt.ReadInterruptSourceReturnsRegister` |
