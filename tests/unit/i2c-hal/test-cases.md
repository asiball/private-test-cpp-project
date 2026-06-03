# 単体テスト仕様書 — i2c-hal

| 項目 | 内容 |
|---|---|
| 対象モジュール | `i2c-hal/` (libi2chal.a) |
| 対象クラス | `I2cDriver`（Linux i2c-dev ラッパー / `II2cDriver` 実装） |
| テストフレームワーク | Google Test |
| 実行コマンド | `./build/test-i2chal/test_i2c_driver` |
| カバレッジ目標 | 主要メソッド 80% 以上 |

実装は `test_i2c_driver.cpp` にあり、各テストケース ID は GTest の
`TEST(Suite, Name)` と 1:1 で対応します。バス/プロトコル仕様は
[i2c-hardware-if.md](../../../docs/deliverables/05_interface-spec/i2c-hardware-if.md) を参照。

> 実機（`/dev/i2c-1`）が無い CI でも走るよう、異常系（未オープン・無効パス）を中心に
> 構成し、実機が要るケースは `GTEST_SKIP()` で自動スキップする。

---

### UT-I2C-001 存在しないパスで `open()` 失敗

| 項目 | 内容 |
|---|---|
| 前提 | デバイス不在 |
| 入力 | `/dev/i2c-no-such` を開く（addr=0x48） |
| 期待 | `open()` false、`is_open()` false、`last_errno()` 非 0 |
| 種別 | 異常系 |
| 実装 | `test_i2c_driver.cpp:I2cDriverOpen.InvalidDeviceReturnsFalse` |

### UT-I2C-002 未オープン時の `write()` は -1 (EBADF)

| 項目 | 内容 |
|---|---|
| 前提 | バス未オープン |
| 入力 | `write(tx, 2)` |
| 期待 | 戻り値 -1、`last_errno()==EBADF` |
| 種別 | 異常系 |
| 実装 | `test_i2c_driver.cpp:I2cDriverWrite.NotOpenReturnsMinusOne` |

### UT-I2C-003 未オープン時の `read()` は -1 (EBADF)

| 項目 | 内容 |
|---|---|
| 前提 | バス未オープン |
| 入力 | `read(rx, 2)` |
| 期待 | 戻り値 -1、`last_errno()==EBADF` |
| 種別 | 異常系 |
| 実装 | `test_i2c_driver.cpp:I2cDriverRead.NotOpenReturnsMinusOne` |

### UT-I2C-004 未オープン時の `write_read()` は -1 (EBADF)

| 項目 | 内容 |
|---|---|
| 前提 | バス未オープン |
| 入力 | `write_read(tx, 1, rx, 2)` |
| 期待 | 戻り値 -1、`last_errno()==EBADF` |
| 種別 | 異常系 |
| 実装 | `test_i2c_driver.cpp:I2cDriverWriteRead.NotOpenReturnsMinusOne` |

### UT-I2C-005 二重 `close()` は安全

| 項目 | 内容 |
|---|---|
| 前提 | 未オープンのインスタンス |
| 入力 | `close()` を 2 回呼ぶ |
| 期待 | クラッシュしない（no-op） |
| 種別 | 正常系 |
| 実装 | `test_i2c_driver.cpp:I2cDriverClose.DoubleCloseIsSafe` |

### UT-I2C-006 コピー禁止確認

| 項目 | 内容 |
|---|---|
| 前提 | — |
| 入力 | 型特性 |
| 期待 | コピー構築 / 代入とも false |
| 種別 | 正常系 |
| 実装 | `test_i2c_driver.cpp:I2cDriverCopyable.IsNotCopyConstructible` |

### UT-I2C-007 有効なバスで `open()`（実機のみ）

| 項目 | 内容 |
|---|---|
| 前提 | `/dev/i2c-1` が存在する実機環境（無ければ `GTEST_SKIP`） |
| 入力 | `/dev/i2c-1` を開く（addr=0x48） |
| 期待 | `open()` true、`is_open()` true |
| 種別 | 正常系（実機のみ） |
| 実装 | `test_i2c_driver.cpp:I2cDriverOpen.ValidBusReturnsTrue` |
