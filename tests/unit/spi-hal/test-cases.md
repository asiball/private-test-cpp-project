# 単体テスト仕様書 — spi-hal

| 項目 | 内容 |
|---|---|
| 対象モジュール | `spi-hal/` (libspihal.a) |
| 対象クラス | `SpiDriver`, `KernelSpiDriver` |
| テストフレームワーク | Google Test |
| 実行コマンド | `./build/test-spihal/test_spi_driver` ほか |
| カバレッジ目標 | 主要メソッド 80% 以上 |

実装はすべて `test_spi_driver.cpp` / `test_kernel_spi_driver.cpp` にあり、
各テストケース ID は GTest の `TEST(Suite, Name)` と 1:1 で対応します。

---

## SpiDriver

### UT-DRV-001 `open()` が有効なデバイスでオープン成功する

| 項目 | 内容 |
|---|---|
| 前提 | `/dev/spidev0.0` が存在する実機環境 |
| 入力 | 有効な spidev パス |
| 期待 | `open()` が true を返し、`is_open()` も true |
| 種別 | 正常系（実機のみ） |
| 実装 | `test_spi_driver.cpp:SpiDriverOpen.ValidDeviceReturnsTrue` |

### UT-DRV-002 `open()` が無効なパスで失敗する

| 項目 | 内容 |
|---|---|
| 前提 | デバイスファイルが存在しない |
| 入力 | `/dev/spidevXX.0` |
| 期待 | `open()` が false を返す |
| 種別 | 異常系 |
| 実装 | `test_spi_driver.cpp:SpiDriverOpen.InvalidDeviceReturnsFalse` |

### UT-DRV-003 `close()` 後は `is_open()` が false になる

| 項目 | 内容 |
|---|---|
| 前提 | デバイス オープン中 |
| 入力 | `close()` 呼び出し |
| 期待 | `is_open()` が false |
| 種別 | 正常系（実機のみ） |
| 実装 | `test_spi_driver.cpp:SpiDriverClose.AfterOpenIsOpenFalse` |

### UT-DRV-004 二重 `close()` が安全に動作する

| 項目 | 内容 |
|---|---|
| 前提 | `close()` を 2 回連続で呼ぶ |
| 入力 | `close(); close();` |
| 期待 | 例外もアボートもなく完走する |
| 種別 | 異常系 |
| 実装 | `test_spi_driver.cpp:SpiDriverClose.DoubleCloseIsSafe` |

### UT-DRV-005 未オープンで `transfer()` を呼ぶと -1 が返る

| 項目 | 内容 |
|---|---|
| 前提 | `open()` していない |
| 入力 | `transfer(tx, rx, len)` |
| 期待 | 戻り値 < 0 |
| 種別 | 異常系 |
| 実装 | `test_spi_driver.cpp:SpiDriverTransfer.NotOpenReturnsMinusOne` |

### UT-DRV-006 コピー禁止確認

| 項目 | 内容 |
|---|---|
| 前提 | — |
| 入力 | `std::is_copy_constructible<SpiDriver>` 等 |
| 期待 | コピー構築 / 代入とも false |
| 種別 | 正常系 |
| 実装 | `test_spi_driver.cpp:SpiDriverCopyable.IsNotCopyConstructible` |

### UT-DRV-007 オープン中に再 `open()` すると失敗する

| 項目 | 内容 |
|---|---|
| 前提 | すでに `open()` 済み |
| 入力 | 再度 `open()` |
| 期待 | false（前の fd をリークしない） |
| 種別 | 異常系（実機のみ） |
| 実装 | `test_spi_driver.cpp:SpiDriverOpen.DoubleOpenWithoutCloseReturnsFalse` |

---

## KernelSpiDriver

### UT-KDRV-001 `open()` が有効なデバイスで成功する

| 項目 | 内容 |
|---|---|
| 前提 | `/dev/my_spi_dev`（my_spi_driver.ko ロード済み） |
| 入力 | 有効なパス |
| 期待 | `open()` が true |
| 種別 | 正常系（実機のみ） |
| 実装 | `test_kernel_spi_driver.cpp:KernelSpiDriverOpen.ValidDeviceReturnsTrue` |

### UT-KDRV-002 `open()` が無効なパスで失敗する

| 項目 | 内容 |
|---|---|
| 前提 | デバイスファイル不在 |
| 入力 | `/dev/my_spi_devXX` |
| 期待 | false |
| 種別 | 異常系 |
| 実装 | `test_kernel_spi_driver.cpp:KernelSpiDriverOpen.InvalidDeviceReturnsFalse` |

### UT-KDRV-003 `open()` 成功後は `is_open()` が true

| 項目 | 内容 |
|---|---|
| 前提 | デバイス取得可 |
| 入力 | `open()` |
| 期待 | `is_open()` true |
| 種別 | 正常系（実機のみ） |
| 実装 | `test_kernel_spi_driver.cpp:KernelSpiDriverOpen.AfterOpenIsOpenTrue` |

### UT-KDRV-004 `close()` 後は `is_open()` が false

| 項目 | 内容 |
|---|---|
| 前提 | オープン後 |
| 入力 | `close()` |
| 期待 | `is_open()` false |
| 種別 | 正常系（実機のみ） |
| 実装 | `test_kernel_spi_driver.cpp:KernelSpiDriverClose.AfterCloseIsOpenFalse` |

### UT-KDRV-005 二重 `close()` が安全

| 項目 | 内容 |
|---|---|
| 前提 | — |
| 入力 | `close(); close();` |
| 期待 | 異常終了しない |
| 種別 | 異常系 |
| 実装 | `test_kernel_spi_driver.cpp:KernelSpiDriverClose.DoubleCloseIsSafe` |

### UT-KDRV-006 未オープンで `transfer()` は -1

| 項目 | 内容 |
|---|---|
| 前提 | `open()` していない |
| 入力 | `transfer(...)` |
| 期待 | 戻り値 < 0 |
| 種別 | 異常系 |
| 実装 | `test_kernel_spi_driver.cpp:KernelSpiDriverTransfer.NotOpenReturnsMinusOne` |

### UT-KDRV-007 コピー禁止確認

| 項目 | 内容 |
|---|---|
| 前提 | — |
| 入力 | 型特性 |
| 期待 | コピー構築 / 代入とも false |
| 種別 | 正常系 |
| 実装 | `test_kernel_spi_driver.cpp:KernelSpiDriverCopyable.IsNotCopyConstructible` |

### UT-KDRV-008 `transfer()` に NULL TX を渡すと -1

| 項目 | 内容 |
|---|---|
| 前提 | オープン済み |
| 入力 | `transfer(nullptr, rx, len)` |
| 期待 | 戻り値 < 0 |
| 種別 | 異常系（実機のみ） |
| 実装 | `test_kernel_spi_driver.cpp:KernelSpiDriverTransfer.NullTxReturnsMinusOne` |

### UT-KDRV-009 `transfer(_, _, 0)` は 0 を返す

| 項目 | 内容 |
|---|---|
| 前提 | オープン済み |
| 入力 | `transfer(tx, rx, 0)` |
| 期待 | 戻り値 == 0 |
| 種別 | 境界値 |
| 実装 | `test_kernel_spi_driver.cpp:KernelSpiDriverTransfer.ZeroLenReturnsZero` |

### UT-KDRV-010 オープン中の再 `open()` は失敗する

| 項目 | 内容 |
|---|---|
| 前提 | オープン済み |
| 入力 | 再度 `open()` |
| 期待 | false |
| 種別 | 異常系（実機のみ） |
| 実装 | `test_kernel_spi_driver.cpp:KernelSpiDriverOpen.DoubleOpenWithoutCloseReturnsFalse` |
