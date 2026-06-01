# GPIOラインインターフェース仕様書

| 項目 | 内容 |
|---|---|
| ドキュメント番号 | IF-GPIO-001 |
| バージョン | 1.0 |
| 作成日 | 2026-06-01 |

> 本書は `gpio`（`GpioLine`）が前提とする **GPIO ラインのエッジ割り込みインターフェース** を規定する。
> Linux の GPIO キャラクタデバイス v2 uABI（`/dev/gpiochipN`）を使用し、sysfs（`/sys/class/gpio`、非推奨）は使わない。

---

## 1. 物理インターフェース

| 信号名 | 方向 | RPi 3B+ ピン例 | 説明 |
|---|---|---|---|
| GPIO ライン | デバイス → ホスト（入力）| 任意の GPIO（例: GPIO17 / Pin 11）| エッジ検出対象。デバイスの割り込み出力を接続 |
| GND | — | Pin 6 等 | 共通グランド |

> 例：ADS1115 の **ALERT/RDY** 出力を GPIO ラインへ接続し、変換完了通知（RDY）を割り込みとして受ける。
> 接続先は `/dev/gpiochip0` の line 17 を例とする（`examples/ads1115_alert_demo.cpp --gpiochip /dev/gpiochip0 --line 17`）。

## 2. 電気的仕様

| 項目 | 仕様 |
|---|---|
| 論理電圧 | 3.3V |
| 入力モード | 入力 + エッジ検出（`GPIO_V2_LINE_FLAG_INPUT`）|
| 検出エッジ | 立ち上がり / 立ち下がり / 両方 |

> プルアップ/プルダウンは接続デバイスの出力形態に合わせる（ADS1115 ALERT/RDY はアクティブ Low 想定のため Falling を使うことが多い）。

## 3. Linux GPIO キャラクタデバイス（v2 uABI）

ラインの要求は `GPIO_V2_GET_LINE_IOCTL` に `gpio_v2_line_request` を渡して行う。成功すると **ライン fd**（`req.fd`）が返り、以降のイベントはこの fd 経由で受け取る。

```c
struct gpio_v2_line_request req;     // 抜粋
req.num_lines    = 1;
req.offsets[0]   = offset;           // GPIO 番号
req.consumer     = "embedded-gpio";  // 消費者名
req.config.flags = GPIO_V2_LINE_FLAG_INPUT
                 | GPIO_V2_LINE_FLAG_EDGE_RISING;   // 検出エッジ
```

| `Edge` | 設定されるフラグ |
|---|---|
| `Rising` | `GPIO_V2_LINE_FLAG_EDGE_RISING` |
| `Falling` | `GPIO_V2_LINE_FLAG_EDGE_FALLING` |
| `Both` | `EDGE_RISING \| EDGE_FALLING` |

## 4. イベントの取得

エッジが発生するとライン fd が読み取り可能になる。`read()` で 1 件の `gpio_v2_line_event`（イベント ID・タイムスタンプ・ライン offset 等）が取れる。複数 fd を 1 ループで捌くため `epoll`（または `poll`）で待つのが標準。

```c
struct gpio_v2_line_event event;
read(line_fd, &event, sizeof(event));   // エッジ発生時に 1 件取り出す
```

## 5. ioctl / システムコール一覧

| 操作 | 用途 |
|---|---|
| `open("/dev/gpiochipN", O_RDONLY \| O_CLOEXEC)` | GPIO チップをオープン |
| `GPIO_V2_GET_LINE_IOCTL` | ラインを入力 + エッジ検出として要求し、ライン fd を得る |
| `epoll_create1` / `epoll_ctl` / `epoll_wait` | ライン fd の読み取り可能（イベント発生）を待つ |
| `read(line_fd, ...)` | `gpio_v2_line_event` を 1 件取り出す |

## 6. バージョン履歴

| バージョン | 変更内容 |
|---|---|
| 1.0 | 初版 |
