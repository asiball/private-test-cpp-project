# API仕様書 — GpioLine

| 項目 | 内容 |
|---|---|
| ドキュメント番号 | API-GPIO-001 |
| バージョン | 1.0 |
| 作成日 | 2026-06-01 |
| ヘッダ | `#include <gpio_line.hpp>` |
| リンク | `-lgpio` |
| 名前空間 | `embedded::` |

> GPIO 1 ラインのエッジ割り込み（イベント）を扱うクラス。
> Linux の GPIO キャラクタデバイス（`/dev/gpiochipN`）の **v2 uABI** を直接 ioctl で叩く（libgpiod 非依存）。
> SPI/I2C の**ポーリング**に対し、こちらは「変化したら通知が来る」**イベント駆動**モデル。`epoll` でイベントまで眠る。

---

## 1. 概要

ADS1115 の ALERT/RDY ピン等と組み合わせ、「`sleep` して待つ」代わりに「変換完了の割り込みで起こされてから読む」構成を作れる。1 本のラインを入力 + エッジ検出として要求し、`wait_event()` でイベントを待つ。

## 2. Edge 列挙

```cpp
enum class GpioLine::Edge { Rising, Falling, Both };
```

| 値 | 検出するエッジ |
|---|---|
| `Rising` | 立ち上がり |
| `Falling` | 立ち下がり |
| `Both` | 両エッジ |

## 3. GpioLine クラス

コピー禁止。スレッドセーフではない。確保した fd はデストラクタで自動 close（RAII）。

### 3.1 コンストラクタ / デストラクタ

```cpp
GpioLine(const std::string& chip_path, unsigned int offset);
~GpioLine();  // 確保した fd を自動 close
```

| パラメータ | 説明 |
|---|---|
| `chip_path` | GPIO チップのパス（例: `"/dev/gpiochip0"`）|
| `offset` | チップ内のラインオフセット（GPIO 番号）|

### 3.2 メソッド一覧

| メソッド | 戻り値 | 説明 |
|---|---|---|
| `request_edge_events(edge)` | `bool` | ラインを入力 + 指定エッジ検出として要求する。成功で true |
| `wait_event(timeout_ms)` | `int` | エッジイベントを `epoll` で待つ。`1`=発生（読み出し済み）/ `0`=タイムアウト / `-1`=エラー。`timeout_ms < 0` で無限待ち |
| `event_fd()` | `int` | ライン fd を返す（外部の epoll ループに組み込む用途）。未要求なら `-1` |
| `close()` | `void` | 確保した fd を閉じる。未確保時は no-op |
| `is_requested()` | `bool` | エッジ検出を要求済みなら true |
| `last_errno()` | `int` | 直近エラーの errno 値 |

---

## 4. 使用例

### 4.1 単純な待ち受け

```cpp
#include <gpio_line.hpp>

embedded::GpioLine alert("/dev/gpiochip0", 17);
if (!alert.request_edge_events(embedded::GpioLine::Edge::Falling)) {
    // errno は alert.last_errno()
}
int r = alert.wait_event(1000);   // 1秒待つ
// r == 1: エッジ発生 / r == 0: タイムアウト / r == -1: エラー
```

### 4.2 ADS1115 ALERT/RDY と組み合わせた割り込み駆動読み出し

```cpp
embedded::Ads1115 adc("/dev/i2c-1");
embedded::GpioLine alert("/dev/gpiochip0", 17);
adc.open();
adc.enable_conversion_ready_pin();
alert.request_edge_events(embedded::GpioLine::Edge::Falling);

for (;;) {
    alert.wait_event(1000);   // sleep ではなく「割り込みで起こされる」
    auto v = adc.read_voltage(0);
}
```

---

## 5. エラーハンドリング / スレッド安全性

- 失敗は戻り値（`false` / `-1` / `0`）で表し、例外は使用しない（`noexcept`）。原因は `last_errno()` で参照。
- `request_edge_events()` 前に `wait_event()` を呼ぶと `EBADF` で `-1`。
- スレッドセーフではない。

## 6. バージョン履歴

| バージョン | 変更内容 |
|---|---|
| 1.0 | 初版。`request_edge_events`, `wait_event`, `event_fd`, `close`, `is_requested`, `last_errno` |
