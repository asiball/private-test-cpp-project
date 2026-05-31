# C 開発者のための C++ ステップアップガイド

> このガイドは「組み込みでは C ばかり書いてきた」「C++ は名前だけ知っている」読者向けです。
> このリポジトリの実コードを題材に、**C で普段やっていることが C++ ではどう書けて、何が嬉しいのか**
> を一歩ずつ橋渡しします。C++ を“難しい言語”として丸暗記するのではなく、
> **「いつもの C のイディオムの言語化・自動化」** として捉えるのが狙いです。

対象コード:
- `spi-hal/` … SPI ドライバ（既存）
- `i2c-hal/` … I2C ドライバ（新規・本ガイドの主な題材）
- `libsensor/` … MCP3008 / ADS1115 センサー
- `gpio/` … GPIO 割り込み

---

## 1. 不透明ポインタ → PIMPL

C で「ヘッダに構造体の中身を見せたくない（ABI を安定させたい / 再コンパイルを減らしたい）」とき、
**不透明ポインタ（opaque pointer）** を使いますよね。

```c
/* C: ヘッダ ads1115.h —— 中身は隠す */
typedef struct ads1115 ads1115_t;
ads1115_t* ads1115_create(const char* path);
void       ads1115_destroy(ads1115_t* self);
int        ads1115_read_raw(ads1115_t* self, uint8_t ch);
```

C++ の **PIMPL（Pointer to IMPLementation）** は、これとまったく同じ発想です。
`libsensor/include/ads1115.hpp` を見てください:

```cpp
class Ads1115 {
public:
    explicit Ads1115(const std::string& i2c_path, uint16_t addr = DEFAULT_ADDR);
    ~Ads1115();
    std::optional<int16_t> read_raw(uint8_t channel);
private:
    struct Impl;                    // ← 中身は宣言だけ（= opaque struct）
    std::unique_ptr<Impl> impl_;    // ← opaque pointer に相当
};
```

`struct Impl` の中身は `libsensor/src/ads1115.cpp` にしか書きません。
**違いは1点だけ**: C なら呼び出し側が `ads1115_destroy()` を忘れず呼ぶ必要がありますが、
C++ では `std::unique_ptr<Impl>` がデストラクタで自動的に解放します（次節 RAII）。

| | C | C++ (PIMPL) |
|---|---|---|
| 中身を隠す | `typedef struct foo foo;` | `struct Impl; unique_ptr<Impl> impl_;` |
| 生成 | `foo_create()` | コンストラクタ |
| 破棄 | `foo_destroy()`（呼び忘れ注意）| デストラクタ（自動）|

---

## 2. 「open したら close」 → RAII

C で一番多いバグの一つが「`open()` したのに、エラー経路で `close()` を忘れる」です。
だから C ではよく `goto err;` で後始末を一箇所に集めます。

```c
int fd = open(path, O_RDWR);
if (fd < 0) return -1;
if (ioctl(fd, ...) < 0) goto err;   /* 後始末を忘れない工夫 */
...
err:
    close(fd);
    return -1;
```

C++ の **RAII（Resource Acquisition Is Initialization）** は、
「リソースの寿命をオブジェクトの寿命に縛る」ことでこの後始末を**言語に任せます**。
`i2c-hal/include/i2c_driver.hpp` / `.cpp`:

```cpp
class I2cDriver {
    ~I2cDriver() { close(); }   // スコープを抜けたら必ず閉じる
};
```

```cpp
{
    embedded::I2cDriver bus("/dev/i2c-1");
    if (!bus.open(0x48)) return;
    bus.write(...);
}   // ← ここで bus が破棄され、自動で close() される。return / 例外でも同じ
```

`goto err;` の後始末が「型」になったもの、と捉えると腑に落ちます。
このリポジトリのドライバはすべて RAII で fd を管理しています。

---

## 3. 関数ポインタの ops 表 → インターフェース（純粋仮想クラス）

C で「実装を差し替えたい」とき（実機ドライバ / モック / 別チップ）、
**関数ポインタを並べた ops 構造体** を作りますよね。

```c
struct i2c_ops {
    int  (*open)(void* self, uint16_t addr);
    int  (*write)(void* self, const uint8_t* d, size_t n);
    int  (*read)(void* self, uint8_t* d, size_t n);
};
/* 実機なら real_ops、テストなら mock_ops を渡す */
```

C++ の **インターフェース（純粋仮想関数だけのクラス）** が、これの言語機能版です。
`i2c-hal/include/ii2c_driver.hpp`:

```cpp
class II2cDriver {
public:
    virtual bool open(uint16_t addr) noexcept = 0;   // = 0 が「純粋仮想」
    virtual int  write(const uint8_t* data, size_t len) noexcept = 0;
    virtual int  read(uint8_t* data, size_t len) noexcept = 0;
    virtual ~II2cDriver() = default;
};
```

コンパイラが各クラスに **vtable（= ops 表）** を自動生成し、`drv->write(...)` を正しい実装へ
ディスパッチします。**手書きの関数ポインタ表が、言語が面倒を見てくれる仕組みになった**のです。

これにより、上位クラス（`Ads1115`）は実機にもモックにも依存しません:

```cpp
embedded::Ads1115 adc("/dev/i2c-1");   // 実機: 内部で I2cDriver を生成
embedded::Ads1115 adc(&mock_driver);   // テスト: MockI2cDriver を差し込む（DI）
```

`tests/mocks/mock_i2c_driver.hpp` が「テスト用 ops」、`I2cDriver` が「実機用 ops」に当たります。

---

## 4. `#define` 定数・マジックナンバー → `enum class` / `constexpr`

C の `#define` はプリプロセッサの単純置換で、型もスコープもありません。

```c
#define GAIN_2_048V  2        /* 型なし・名前衝突しうる */
```

C++ では **`enum class`**（型安全で名前空間を持つ列挙）と **`constexpr`**（型付きコンパイル時定数）を使います。
`libsensor/include/ads1115.hpp`:

```cpp
enum class Gain : uint8_t { FSR_6_144V, FSR_4_096V, FSR_2_048V, /* ... */ };
static constexpr uint16_t DEFAULT_ADDR = 0x48;
```

`Gain::FSR_2_048V` は `int` と暗黙に混ざらず、`switch` の case 漏れも警告できます。

---

## 5. エラーは戻り値 `-1` → `std::optional`（C++17）

C では「失敗したら -1 / NULL」を返し、呼び出し側がチェックを忘れがちです。
C++17 の **`std::optional<T>`** は「値があるか無いか」を型で表します。

```cpp
std::optional<int16_t> raw = adc.read_raw(0);
if (raw) {                 // 値があるか明示的に確認
    use(*raw);
}
```

戻り値の確認忘れは `[[nodiscard]]`（戻り値を捨てると警告）でさらに防げます。
本リポジトリの `read_*` / `transfer` は `[[nodiscard]]` 付きです。

> 補足: C++23 には成功/失敗を一つの型で運べる `std::expected` がありますが、本プロジェクトは
> **C++17** を対象にしているため `std::optional` + `last_errno()` の組み合わせを採用しています。

---

## 6. ファイルスコープ `static` → 無名 namespace

C で「この .c の中だけで使う関数・定数」に `static` を付けますよね。
C++ では **無名 namespace** が同じ役割（内部リンケージ）を、より一般的に担います。
`libsensor/src/ads1115.cpp`:

```cpp
namespace {   // この翻訳単位の中だけで有効
constexpr uint8_t REG_CONFIG = 0x01;
constexpr uint16_t pga_bits(Ads1115::Gain g) { /* ... */ }
}
```

---

## 7. ここから先のステップ

このリポジトリで「C の発想 → C++ の機能」を一通り体験したら、次の一歩としては:

1. **同じ抽象をもう一度引き直す**: `ISpiDriver` と `II2cDriver` を見比べ、共通点（open/close/転送/エラー）を
   `IBus` のような共通インターフェースにまとめられないか考えてみる（演習に最適）。
2. **テンプレート**: ops 表を実行時 vtable ではなくコンパイル時に解決する（ゼロオーバーヘッド）方法。
3. **C++20 以降**: `std::span`（ポインタ+長さの安全な束ね）、`std::jthread`、`std::expected` など。
   ※本プロジェクトは C++17 のため未使用。「次に学ぶと良いもの」として把握しておく。

関連ドキュメント:
- [I2C と ADS1115](i2c-and-ads1115.md) — バスとセンサーの追加例
- [GPIO 割り込みと epoll](gpio-interrupts-epoll.md) — ポーリング vs イベント駆動
- [学習ガイド](learning-guide.md) — プロジェクト全体の歩き方
