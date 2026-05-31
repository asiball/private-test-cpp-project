# GPIO 割り込みと epoll —— ポーリング vs イベント駆動

> SPI/I2C のセンサー読み出しは「こちらから叩いて結果を取る（ポーリング）」モデルでした。
> GPIO のエッジ検出は「相手が変化したら通知が来る（イベント駆動）」という**別のモデル**です。
> ここでは両者の違いと、Linux で割り込みを待つ標準的な方法（GPIO chardev + epoll）を学びます。
> `gpio/` コンポーネントは外部ライブラリに依存せず、単体でビルドできます。

---

## 1. ポーリング vs 割り込み

```
ポーリング:                          割り込み（イベント駆動）:
  while (true) {                       epoll_wait(...);   // イベントまで眠る（CPU 0%）
      read_sensor();                   read_sensor();     // 起きたら読む
      sleep(10ms);   // CPU を消費       
  }
```

- **ポーリング**: 実装が単純。だが「無駄に何度も読む」か「sleep して取りこぼす」かのトレードオフ。
- **割り込み**: 変化があった時だけ動くので低消費電力・低レイテンシ。実装はやや複雑。

ADS1115 の `read_raw()` は内部で Config の OS ビットをポーリングしています（[I2C/ADS1115 ガイド](i2c-and-ads1115.md)）。
これを **ALERT/RDY ピン + GPIO 割り込み**に置き換えると、「変換完了で起こされてから読む」構成になります。

---

## 2. Linux の GPIO キャラクタデバイス（v2 uABI）

かつての `/sys/class/gpio`（sysfs）は非推奨です。現在は **`/dev/gpiochipN`** に対し
`ioctl` でラインを要求し、エッジイベントを fd 経由で受け取ります。`gpio/src/gpio_line.cpp`:

```cpp
struct gpio_v2_line_request req{};
req.num_lines  = 1;
req.offsets[0] = offset;                       // GPIO 番号
req.config.flags = GPIO_V2_LINE_FLAG_INPUT
                 | GPIO_V2_LINE_FLAG_EDGE_RISING;   // 立ち上がりで通知
ioctl(chip_fd, GPIO_V2_GET_LINE_IOCTL, &req);  // req.fd にライン fd が返る
```

返ってきた **ライン fd** は、エッジが起きると読み取り可能になります。
`read()` すると `struct gpio_v2_line_event` が 1 件取れます。

---

## 3. epoll で「イベントまで眠る」

`epoll` は「複数の fd を監視し、どれかが ready になるまでブロックする」Linux の仕組みです。
`GpioLine::wait_event()` の中核:

```cpp
int epfd = epoll_create1(EPOLL_CLOEXEC);
struct epoll_event ev{ EPOLLIN, { .fd = line_fd_ } };
epoll_ctl(epfd, EPOLL_CTL_ADD, line_fd_, &ev);

struct epoll_event out;
int n = epoll_wait(epfd, &out, 1, timeout_ms);   // n>0: 発生, 0: timeout, -1: error
if (n > 0) {
    struct gpio_v2_line_event event;
    read(line_fd_, &event, sizeof(event));        // イベントを取り出す
}
```

> このプロジェクトでは 1 本のラインだけ監視するので `poll()` でも十分ですが、
> 「複数のセンサー割り込み + タイマー fd + ソケット」を 1 ループで捌くような実務では `epoll` が定番です。
> `event_fd()` を公開しているので、外部の大きな epoll ループに組み込むこともできます。

---

## 4. ADS1115 ALERT/RDY との連携

ADS1115 は ALERT/RDY ピンを「変換完了通知（RDY）」として使えます
（Hi_thresh の MSB=1 / Lo_thresh の MSB=0）。`Ads1115::enable_conversion_ready_pin()` がこれを設定します。

組み合わせると、ポーリングの `sleep` を割り込み待ちに置き換えられます:

```cpp
embedded::Ads1115 adc("/dev/i2c-1");
embedded::GpioLine alert("/dev/gpiochip0", 17);
adc.open();
adc.enable_conversion_ready_pin();
alert.request_edge_events(embedded::GpioLine::Edge::Falling);

for (;;) {
    alert.wait_event(1000);        // sleep ではなく「割り込みで起こされる」
    auto v = adc.read_voltage(0);
}
```

動く例は `examples/ads1115_alert_demo.cpp` にあります（`--gpiochip` / `--line` を渡すと割り込みモード）:

```
./ads1115_alert_demo --device /dev/i2c-1 --channel 0 --gpiochip /dev/gpiochip0 --line 17
```

---

## 5. テストと実機

`tests/unit/gpio/test_gpio_line.cpp` は、実機が無くても確認できる範囲
（未要求での `wait_event` が `EBADF`、不正チップで失敗、二重 close 安全、コピー禁止）を CI で検証し、
実機が要る項目（実際のエッジ要求）は `GTEST_SKIP()` でスキップします。
これは spi-hal / i2c-hal のテストと同じ方針です。

関連: [C→C++ ステップアップガイド](c-to-cpp-stepping-stones.md) / [I2C と ADS1115](i2c-and-ads1115.md)
