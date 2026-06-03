# 単体テスト仕様書 — gpio

| 項目 | 内容 |
|---|---|
| 対象モジュール | `gpio/` (libgpio.a) |
| 対象クラス | `GpioLine`（GPIO chardev v2 uABI / epoll エッジ割り込み） |
| テストフレームワーク | Google Test |
| 実行コマンド | `./build/test-gpio/test_gpio_line` |
| カバレッジ目標 | 主要メソッド 80% 以上 |

実装は `test_gpio_line.cpp` にあり、各テストケース ID は GTest の
`TEST(Suite, Name)` と 1:1 で対応します。ライン要求/イベント仕様は
[gpio-line-if.md](../../../docs/deliverables/05_interface-spec/gpio-line-if.md) を参照。

> 実機（`/dev/gpiochip0`）が無い CI でも走るよう、異常系（未要求・無効チップ）を中心に
> 構成し、実機が要るケースは `GTEST_SKIP()` で自動スキップする。

---

### UT-GPIO-001 要求前の `wait_event()` は -1 (EBADF)

| 項目 | 内容 |
|---|---|
| 前提 | `request_edge_events()` 未呼び出し |
| 入力 | `wait_event(10)` |
| 期待 | 戻り値 -1、`last_errno()==EBADF` |
| 種別 | 異常系 |
| 実装 | `test_gpio_line.cpp:GpioLineWait.NotRequestedReturnsMinusOne` |

### UT-GPIO-002 存在しないチップで `request_edge_events()` 失敗

| 項目 | 内容 |
|---|---|
| 前提 | チップデバイス不在 |
| 入力 | `/dev/gpiochip-no-such` に対し `request_edge_events(Rising)` |
| 期待 | false、`is_requested()` false、`last_errno()` 非 0 |
| 種別 | 異常系 |
| 実装 | `test_gpio_line.cpp:GpioLineRequest.InvalidChipReturnsFalse` |

### UT-GPIO-003 二重 `close()` は安全

| 項目 | 内容 |
|---|---|
| 前提 | 未要求のインスタンス |
| 入力 | `close()` を 2 回呼ぶ |
| 期待 | クラッシュしない（no-op） |
| 種別 | 正常系 |
| 実装 | `test_gpio_line.cpp:GpioLineClose.DoubleCloseIsSafe` |

### UT-GPIO-004 コピー禁止確認

| 項目 | 内容 |
|---|---|
| 前提 | — |
| 入力 | 型特性 |
| 期待 | コピー構築 / 代入とも false |
| 種別 | 正常系 |
| 実装 | `test_gpio_line.cpp:GpioLineCopyable.IsNotCopyConstructible` |

### UT-GPIO-005 実機の gpiochip でエッジ要求（実機のみ）

| 項目 | 内容 |
|---|---|
| 前提 | `/dev/gpiochip0` が存在する実機環境（無ければ `GTEST_SKIP`） |
| 入力 | `request_edge_events(Both)` |
| 期待 | 成功時は `is_requested()` true かつ `event_fd() >= 0`（ライン使用中なら skip） |
| 種別 | 正常系（実機のみ） |
| 実装 | `test_gpio_line.cpp:GpioLineRequest.ValidChipRequestsEvents` |
