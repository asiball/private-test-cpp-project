# C++ 組み込みプロジェクトの Rust 移行ガイド

> **対象読者**: C/C++ を主言語とする組み込みエンジニア。Rust の知識はゼロでも読めるように書いています。
> **対応コード**: このリポジトリの `rust/` ディレクトリが実際の実装例です。

---

## 目次

1. [Rust とは何か (C エンジニア向け 3 分解説)](#1-rust-とは何か)
2. [C++ パターンと Rust パターンの対応表](#2-設計パターンの対応)
3. [メリット: Rust に置き換えると何が良くなるか](#3-メリット)
4. [デメリット・コスト: 何が難しくなるか](#4-デメリットとコスト)
5. [具体的な移行コストの見積もり](#5-移行コストの見積もり)
6. [このプロジェクトで実際に発見されたバグと Rust での対処](#6-実際に発見されたバグ)
7. [移行戦略: 全書き換えか部分移行か](#7-移行戦略)
8. [ローカルで試す最短手順](#8-ローカルで試す)
9. [組み込み Rust 完全解説 — Web バックエンドだけではない](#9-組み込み-rust-完全解説webバックエンドだけではない)

---

## 1. Rust とは何か

### C エンジニアへの一言説明

> **「GC なし・ランタイムなしで、コンパイル時にメモリ安全を保証する言語」**

C/C++ との最大の違いは **所有権 (Ownership)** という仕組みです。

```c
// C: ポインタを渡しても「誰が free するか」はコンパイラが関知しない
char *buf = malloc(128);
process(buf);      // buf はまだ使える? free された? → 実行時まで不明
free(buf);
process(buf);      // Use-After-Free — コンパイラは検出しない
```

```rust
// Rust: 所有権がコンパイル時に追跡される
let buf = vec![0u8; 128];
process(buf);      // buf の所有権が process に移動 (move)
process(buf);      // コンパイルエラー: buf はもう使えない
```

**組み込みエンジニアに関係する具体的な特徴:**

| 機能 | C | C++ | Rust |
|---|---|---|---|
| ガベージコレクション | なし | なし | **なし** |
| ランタイム | libc のみ | libc + libstdc++ | **libc のみ** (no_std も可) |
| メモリ管理 | 手動 | RAII (手動 + スマートポインタ) | **所有権 (コンパイル時保証)** |
| データ競合 | 実行時 | 実行時 (ThreadSanitizer で一部検出) | **コンパイル時エラー** |
| Null 参照 | 実行時クラッシュ | 実行時クラッシュ | **コンパイル時に不可能** |
| バイナリサイズ | 小 | 中 | 小〜中 |
| 組み込み (no_std) | ○ | △ | ○ |

---

## 2. 設計パターンの対応

> **C のみのエンジニアへ:** このセクションは C++ コードと Rust コードを並べて比較しています。
> C++ を知らなくても読めるように各所に補足を入れています。
> 以下は本セクションで登場する C++ 用語のクイックリファレンスです。
>
> | C++ 用語 | 意味 | C での近似 |
> |---|---|---|
> | 純粋仮想クラス | 実装を持たない「型の約束書き」 | 関数ポインタを並べた構造体 |
> | vtable | 仮想関数の実装先アドレスを格納するテーブル | 関数ポインタの配列 |
> | PIMPL | ヘッダに実装詳細を出さないイディオム | 不完全型ポインタ (`struct Foo;`) |
> | ABI | バイナリレベルの互換性 (`.so` の差し替え可能性) | — |
> | RAII | リソース取得=初期化、解放=デストラクタで自動化 | `goto err` クリーンアップ |
> | `std::optional<T>` | 値が「ある/ない」を表す型 | 番兵値 (`-1`, `NULL`) |
> | `[[nodiscard]]` | 戻り値を無視してはいけない属性 | (C23 の `[[nodiscard]]` と同等) |
> | `noexcept` | 「この関数は例外を投げない」宣言 | C は例外がないので不要 |

---

### 2.1 インターフェース (純粋仮想クラス → トレイト)

C では「インターフェース」を**関数ポインタを持つ構造体**で模倣します。
C++ の純粋仮想クラスは、これを言語機能として提供したものです。

```c
// C: 関数ポインタのテーブルで「インターフェース」を模倣
typedef struct {
    int  (*open)    (void *ctx, uint32_t speed_hz);
    int  (*transfer)(void *ctx, const uint8_t *tx, uint8_t *rx, size_t len);
    int  (*is_open) (void *ctx);
    void *ctx;   // ← "self" に相当するコンテキストポインタ
} SpiDriver;
```

```cpp
// C++: ISpiDriver (spi-hal/include/ispi_driver.hpp)
// 「純粋仮想クラス」= 実装を持たない、型の約束書き
class ISpiDriver {
public:
    virtual ~ISpiDriver() = default;
    [[nodiscard]] virtual bool open(const Config& cfg) noexcept = 0;
    [[nodiscard]] virtual int transfer(
        const uint8_t* tx, uint8_t* rx, size_t len) noexcept = 0;
    [[nodiscard]] virtual bool is_open() const noexcept = 0;
};
```

Rust では **トレイト (trait)** が同じ役割を果たします。

```rust
// Rust: SpiDriver (rust/spi-hal/src/lib.rs)
pub trait SpiDriver: Send {
    // &mut self = C の「第一引数 void *ctx (書き込み可)」に相当
    fn open(&mut self, config: &SpiConfig) -> Result<(), SpiError>;
    fn transfer(&mut self, tx: &[u8], rx: &mut [u8]) -> Result<(), SpiError>;
    // &self = C の「第一引数 const void *ctx (読み取り専用)」に相当
    fn is_open(&self) -> bool;
}
```

**違い:**
- C の関数ポインタテーブル / C++ の vtable (仮想関数アドレステーブル) → Rust の `dyn Trait` (動的ディスパッチ) またはジェネリクス (静的ディスパッチ)
- C++ の `[[nodiscard]]` → Rust の `#[must_use]` (戻り値を無視するとコンパイル警告)
- C の `errno` + 戻り値 `-1` / C++ の `noexcept` + bool 戻り値 → Rust の `Result<T, E>` (成功/失敗を型で表現)
- `&mut self` : 自分自身への可変参照 (C の `void *ctx` で状態を書き換えることに相当)
- `&self` : 自分自身への不変参照 (C の `const void *ctx` に相当)

---

### 2.2 PIMPL パターン → 不要になる

**PIMPL (Pointer to IMPLementation)** とは、ヘッダに実装の詳細を出さないための C++ のイディオムです。
C でも「不完全型ポインタ」で同じことをします。

```c
// C: 不完全型ポインタで実装詳細をヘッダに出さない (C の PIMPL 相当)
// sensor.h — 公開ヘッダ。struct の中身は知らせない
typedef struct SensorImpl Sensor;
Sensor *sensor_new(const char *spi_path, double vref);
int     sensor_read_raw(Sensor *s, uint8_t channel, uint16_t *out);
void    sensor_free(Sensor *s);

// sensor.c — 実装ファイルにのみ struct の中身を定義
struct SensorImpl { int fd; double vref; /* OS 依存フィールドなど */ };
```

C++ の PIMPL はこれを `unique_ptr<Impl>` (ヒープ上の自動解放ポインタ) でやります。

```cpp
// C++: sensor.hpp (公開ヘッダ — 実装詳細を隠蔽)
class Sensor {
public:
    Sensor(const std::string& spi_path, double vref = 3.3);
    [[nodiscard]] std::optional<uint16_t> read_raw(uint8_t channel) noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;  // ← PIMPL: ヒープ上の実装へのポインタ
};
```

```cpp
// C++: sensor.cpp (実装 — ヘッダに出ない)
struct Sensor::Impl {
    ISpiDriver* driver;
    bool owns_driver;
    double vref_volts;
    // ... OS 依存ヘッダ、実装詳細
};
```

Rust では **モジュール境界が可視性を制御する** ため、PIMPL は不要です。

```rust
// Rust: mcp3008.rs — pub でないフィールドはモジュール外から見えない
pub struct Mcp3008 {
    // Box<dyn SpiDriver>:
    //   Box<T>       = ヒープ確保 + 自動解放 (C の malloc+free を自動化)
    //   dyn SpiDriver = SpiDriver トレイトを実装した「何か」を実行時に解決
    //   C の「void *ctx + 関数ポインタテーブルへのポインタ」に相当
    driver: Box<dyn SpiDriver>,
    vref: f64,
    open: bool,
}
```

**効果:**
- ヘッダファイルが不要 → コンパイル単位の概念自体が変わる
- `unique_ptr` の `new/delete` も不要 (C では `sensor_new`/`sensor_free` に相当)
- `owns_driver` フラグも不要 (所有権をコンパイラが追跡)

---

### 2.3 依存性注入 (DI) → Box<dyn Trait>

**依存性注入 (Dependency Injection)** とは、テストと本番で「実装の中身」を差し替えるパターンです。
C では関数ポインタを引数で渡すことで実現します。

```c
// C: 関数ポインタ構造体を外から渡すことで「モック」を注入
int sensor_init(SpiDriver *driver, double vref);  // driver は外から差し込む

// テストでは偽の SpiDriver を渡す:
SpiDriver mock = { .transfer = mock_transfer_fn, .ctx = &mock_state };
sensor_init(&mock, 3.3);
```

```cpp
// C++: GTest/GMock (C++ 向けテストフレームワーク) でモックを注入
Sensor(ISpiDriver* driver, double vref); // テスト用コンストラクタ
Sensor(const std::string& path, double vref); // 本番用コンストラクタ

MockSpiDriver mock;                              // モック(偽ドライバ)を生成
EXPECT_CALL(mock, transfer(...)).WillOnce(Return(3)); // 返す値を事前設定
Sensor s(&mock, 3.3);
```

```rust
// Rust: Box<dyn SpiDriver> でモックを注入 (C の関数ポインタ構造体渡しに相当)
pub fn with_driver(driver: Box<dyn SpiDriver>, vref: f64) -> Self { ... }

// テストでは手書きの MockSpiDriver を渡す:
let (mock, tx_log) = MockSpiDriver::new();
mock.push_response(vec![0x00, 0x01, 0xFF]);  // 返すバイト列をあらかじめセット
let mut sensor = Mcp3008::with_driver(Box::new(mock), 3.3);
```

**GTest/GMock との違い:**
- GTest/GMock は C++ 向けのテスト・モックフレームワーク。Rust には `cargo test` が標準で付属。
- Rust にはマクロベースのモックフレームワーク (`mockall` クレート) もあるが、
  手書きモックも簡単なので小規模プロジェクトでは不要

---

### 2.4 RAII → Drop トレイト

**RAII (Resource Acquisition Is Initialization):** C++ のイディオムで、
「リソースの取得=オブジェクト初期化、リソースの解放=デストラクタで自動化」すること。
C での `goto err` クリーンアップパターンと同じ目的です。

```c
// C: goto err でリソースを確実に解放する (RAII の手動版)
int do_work(void) {
    int fd = open("/dev/spidev0.0", O_RDWR);
    if (fd < 0) return -1;

    uint8_t *buf = malloc(128);
    if (!buf) { close(fd); return -1; }  // 忘れると fd がリーク

    // ... 処理 ...
    free(buf);
    close(fd);
    return 0;
}
```

```cpp
// C++: デストラクタでスコープを抜けると自動で close() が呼ばれる
SpiDriver::~SpiDriver() {  // デストラクタ = スコープ終了時に自動呼び出し
    if (fd_ >= 0) close(fd_);
}
```

```rust
// Rust: Drop トレイトで同じことを実現
// スコープを抜けると自動で drop() が呼ばれる (C++ のデストラクタと同等)
impl Drop for LinuxSpiDriver {
    fn drop(&mut self) {
        self.close();  // fd を閉じる — goto err が不要になる
    }
}
```

**違い:** Rust では `Drop` の実装忘れがコンパイルエラーにはならないが、
`File` や `Vec` などの標準型は自動でリソース解放する。
カスタムリソース (fd、ioctl ハンドルなど) は明示的に `Drop` を実装する必要がある。

---

### 2.5 エラー処理 → Result<T, E>

C では `errno` + 戻り値 `-1` でエラーを伝えます。これが Rust では型安全になります。

```c
// C: errno + 戻り値 -1 でエラーを伝える
int fd = open("/dev/spidev0.0", O_RDWR);
if (fd < 0) {
    perror("open failed");  // errno を参照して表示 — 呼び忘れると原因不明のまま
    return -1;
}
// 問題: errno は次の関数呼び出しで上書きされる
//       戻り値を無視してもコンパイラは何も言わない
```

```cpp
// C++: 成否を bool で返し、詳細は last_errno() で取る
[[nodiscard]] bool open(const Config& cfg) noexcept;
int last_errno() const noexcept;  // 呼び忘れると情報が消える
```

```rust
// Rust: 成功値とエラーを 1 つの型 Result<T, E> にまとめて返す
// Result<(), SpiError> は「成功なら空値 ()、失敗なら SpiError」という型
pub fn open(&mut self, config: &SpiConfig) -> Result<(), SpiError>;

// 呼び出し側:
sensor.open(&cfg)?;  // ? 演算子 = エラーなら即座に呼び出し元へ return Err(...)
                     // C で書けば: if (ret < 0) return ret; の連続に相当
// または明示的に分岐:
match sensor.open(&cfg) {
    Ok(()) => { /* 成功 */ }
    Err(SpiError::Open(e)) => eprintln!("開けません: {e}"),
    Err(e) => return Err(e),
}
```

**型付きエラーの恩恵:**
- `errno` の上書き問題がない (エラー情報は戻り値の中に入っている)
- `Result` を無視するとコンパイル警告 (`#[must_use]`)
- エラーの種類をパターンマッチで網羅的に処理 (ケース漏れが警告になる)
- `thiserror` クレートで自動的にエラー文字列 (`Display`) を実装

---

### 2.6 std::optional → Option<T>

C では「値がない」を番兵値 (`-1`, `NULL`, `0xFFFF`) で表しますが、
見落としても検出できません。

```c
// C: 番兵値 -1 で「値なし」を表現
int16_t read_sensor(uint8_t ch);  // 失敗時は -1 を返す

int16_t v = read_sensor(0);
// -1 チェックを忘れても動いてしまう (バグの温床)
use_value((uint16_t)v);
```

```cpp
// C++: std::optional<T> = 「値がある / ない」を型で表現
// T には実際の値の型を入れる。uint16_t なら 0〜65535 を正常値として扱える
[[nodiscard]] std::optional<uint16_t> read_raw(uint8_t channel) noexcept;

auto val = sensor.read_raw(0);
if (val) { use(*val); }  // val を確認せず *val を使おうとするとコンパイル警告
```

```rust
// Rust: Option<T> = C++ の std::optional<T> とほぼ同等
// Some(value) = 値がある、None = 値がない
pub fn read_raw(&mut self, channel: u8) -> Result<u16, SensorError>;

// 実際の rust/libadxl345 の read_reg は失敗を Result で表すため戻り値は u8:
pub fn read_reg(&mut self, addr: u8) -> Result<u8, Adxl345Error>;

// Option<T> は「エラーではなく値が無いのが正常」な場合に使う（例示）:
// fn find_calibration(&self, id: u8) -> Option<i16>;  // 該当無しは None
```

**Rust の `Option<T>` は C++ の `std::optional<T>` とほぼ同じ。**
`None` の中身を取り出す前に必ず存在チェックが必要で、忘れるとコンパイルエラー。
番兵値の見落としがコンパイル時に防がれる。

---

### 2.7 スレッド安全 → コンパイル時保証

C では `pthread_mutex_t` + `volatile` 変数でスレッド間共有を管理しますが、
ロック忘れをコンパイラは検出できません。

```c
// C: pthread で共有フラグを管理 — ロック忘れは実行時まで発覚しない
static volatile bool     stop_flag = false;
static pthread_mutex_t   mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t    cv  = PTHREAD_COND_INITIALIZER;

// ← mutex と flag が別々に存在。「セットで使う」はプログラマの約束事
pthread_mutex_lock(&mtx);
stop_flag = true;          // ロックを忘れるとデータ競合 — valgrind/TSAN で事後検出
pthread_cond_signal(&cv);
pthread_mutex_unlock(&mtx);
```

```cpp
// C++: 3 つの変数をセットで管理するのは人間のルール
std::atomic<bool> stop_flag_;
std::mutex mtx_;
std::condition_variable cv_;
```

```rust
// Rust: Mutex に包まれていないデータは別スレッドに送れない (コンパイルエラー)
// Arc<T>    = 複数スレッドで共有できる参照カウントポインタ (pthread の共有変数に相当)
//             C++ の shared_ptr のスレッド安全版
// Mutex<T>  = データ T をロックで保護する型 (pthread_mutex_t 相当)
//             ロックなしでデータにアクセスするコードはコンパイルエラー
// Condvar   = 条件変数 (pthread_cond_t 相当)
let pair = Arc::new((Mutex::new(false), Condvar::new()));
// ↑ 停止フラグ(bool)と条件変数が 1 つの Arc に入っているため、
//   「セットで使う」がコンパイラによって強制される
```

**`Send` / `Sync` トレイト:**
- `Send`: 別スレッドに所有権を移せる型 (スレッド間で値を渡してよい)
- `Sync`: 複数スレッドから同時に参照できる型 (共有読み取りが安全)
- これを実装していない型を別スレッドに渡そうとするとコンパイルエラー
- TSAN (ThreadSanitizer) なしで同等の保証が得られる

---

## 3. メリット

### 3.1 メモリ安全 (コンパイル時保証)

組み込みで頻出するバグのカテゴリと Rust での対処:

| バグの種類 | C/C++ での発見タイミング | Rust での対処 |
|---|---|---|
| Use-After-Free | 実行時クラッシュ / ASAN | **コンパイルエラー** |
| Double Free | 実行時クラッシュ / ASAN | **コンパイルエラー** |
| バッファオーバーフロー (スライス) | 実行時クラッシュ | 実行時パニック (デバッグ) / チェック可能 |
| NULL 参照 | 実行時クラッシュ | **コンパイル時に null 参照は存在しない** (`Option` を使う) |
| データ競合 | 実行時 / TSAN | **コンパイルエラー** |
| 部分書き込みの無視 | 実行時の誤動作 (検出困難) | `write_all()` や `Result` で強制 |

> **実例 (このプロジェクト):** I2C の `write()` が部分書き込みを無視していた (Bug #3)。
> Rust で `write_all()` に変更することで、コンパイル後は部分書き込みが起きない実装になった。

### 3.2 エラー処理の強制

`Result<T, E>` を `?` で伝播するパターンにより、
エラーを無視したコードは `#[must_use]` の警告が出る。

```rust
// これはコンパイル警告が出る
sensor.read_raw(0);  // warning: unused Result

// 明示的に無視するなら let _ = ... と書かなければならない
let _ = sensor.read_raw(0);
```

### 3.3 ゼロコスト抽象

Rust のジェネリクスは**モノモーフィズム (単相化)** で展開されます。
これは「型ごとに別のコードを生成してインライン化」する仕組みで、
C のマクロや `void *` + 関数ポインタと比べて実行時オーバーヘッドがありません。

```c
// C: void * + 関数ポインタで汎用化 → 実行時に間接呼び出し (コスト有)
void read_with_driver(SpiDriver *d) {
    d->transfer(d->ctx, tx, rx, len);  // ← 関数ポインタ経由
}
```

```rust
// 動的ディスパッチ (C の関数ポインタ経由呼び出し / C++ の仮想関数と同等)
// dyn Trait = 実行時にどの実装を呼ぶか決める → 関数ポインタテーブル経由の呼び出し
fn read_with_driver(d: &mut Box<dyn SpiDriver>) { ... }

// 静的ディスパッチ (C のマクロ / C++ のテンプレートと同等): ゼロコスト
// <D: SpiDriver> = コンパイル時に型が確定し、直接呼び出しにインライン化される
fn read_with_driver<D: SpiDriver>(d: &mut D) { ... }
```

どちらを選ぶかはトレードオフです:
- `dyn Trait` → バイナリサイズ小、実行時コスト微増 (関数ポインタ経由)
- ジェネリクス → 実行時ゼロコスト、バイナリサイズ増 (型ごとにコードが生成される)

### 3.4 組み込み (no_std) サポート

`#![no_std]` を付ければ `std` ライブラリなしでビルドできます。
RTOS や bare-metal 環境での利用が可能です。
このリポジトリの `libsensor` / `libadxl345` は `std` を使っていますが、
`std::thread::sleep` を除けば `no_std` 対応は容易です。

詳細は [§9 組み込み Rust 完全解説](#9-組み込み-rust-完全解説webバックエンドだけではない) を参照してください。

### 3.5 ツールチェーンの統一

```sh
cargo build    # ビルド (CMake + make の代替)
cargo test     # テスト (GTest の代替)
cargo clippy   # 静的解析 (cppcheck + clang-tidy の代替)
cargo fmt      # コードフォーマット (clang-format の代替)
cargo doc      # ドキュメント生成 (Doxygen の代替)
```

CMakeLists.txt、.clang-format、Doxyfile、cppcheck の設定ファイルが
すべて `Cargo.toml` と `clippy.toml` に統合されます。

---

## 4. デメリットとコスト

### 4.1 学習曲線が急峻

**所有権・ライフタイム・借用** という概念は C/C++ にはなく、初学者がつまずく点です。

```rust
// エラー例: 値を move した後に使おうとした
let buf = vec![0u8; 128];
let buf2 = buf;   // buf の所有権が buf2 に移動
println!("{:?}", buf); // コンパイルエラー: buf はすでに移動済み

// 解決: 参照 (&) を使う
let buf = vec![0u8; 128];
let buf2 = &buf;  // 借用 (borrow) — buf はまだ使える
println!("{:?}", buf);
```

**組み込みエンジニア向けの学習リソース:**
- [The Rust Programming Language (日本語版)](https://doc.rust-jp.rs/book-ja/)
- [Rust by Example](https://doc.rust-lang.org/rust-by-example/)
- [Embedded Rust Book](https://docs.rust-embedded.org/book/)

**習得期間の目安:**
- 基本的な Rust コードが書けるまで: 1〜2 ヶ月 (C 経験者)
- 自信を持ってレビューできるまで: 3〜6 ヶ月

### 4.2 非同期処理のモデルが大きく変わる

C++ の `std::thread` + `condition_variable` は Rust でも使えますが、
`async/await` を使う場合は `tokio` や `async-std` などのランタイムが必要です。

```cpp
// C++: std::thread でシンプルに非同期読み出し
void Sensor::read_raw_async(uint8_t channel, ReadCallback cb) {
    std::thread([=]() { cb(read_raw(channel)); }).detach();
}
```

```rust
// Rust: async/await の場合は tokio が必要
// tokio は Cargo.toml に追加が必要
async fn read_raw_async(&mut self, channel: u8) -> Result<u16, SensorError> {
    tokio::task::spawn_blocking(move || { ... }).await?
}
```

組み込みの場合、`tokio` は重すぎることが多いため、`embassy` などの組み込み向け
async ランタイムを使うか、素の `std::thread` を使います。

### 4.3 Linux カーネルモジュールは別問題

このプロジェクトには `kernel/my_spi_driver.ko` があります。
Linux カーネルモジュールの Rust 対応 (`rust-for-linux`) は 6.1 以降で正式サポートされましたが、
まだ API が安定しておらず、既存の C カーネルモジュールの 1:1 移行は現実的ではありません。

**推奨:** カーネルモジュールは C のまま残し、ユーザースペースのドライバ層から Rust 化する。

### 4.4 C/C++ 独自機能の代替が必要

| C/C++ | Rust での代替 | 補足 |
|---|---|---|
| 番兵値 (`-1`, `NULL`) | `Option<T>` | `None` の見落としがコンパイルエラー |
| `errno` + 戻り値 `-1` | `Result<T, E>` | エラーの種類が型として残る |
| `goto err` クリーンアップ | `Drop` トレイト | スコープ抜けで自動実行 |
| `void *` + 関数ポインタ | `Box<dyn Trait>` | 型安全な動的ディスパッチ |
| `#define` 定数 | `const` / `enum` | 型付き、スコープあり |
| `std::optional<T>` | `Option<T>` — ほぼ同等 | — |
| `std::variant<T...>` | `enum` (タグ付きユニオン) | — |
| `std::string_view` | `&str` | — |
| `std::span<T>` | `&[T]` | — |
| テンプレート | ジェネリクス + トレイト | — |
| `constexpr` | `const` / `const fn` | — |

### 4.5 C ライブラリとの相互運用 (FFI)

**FFI (Foreign Function Interface):** 異なる言語間でコードを呼び合う仕組み。
既存の C ライブラリを Rust から呼ぶには `unsafe` ブロックが必要です。

```rust
// libc の open/ioctl を呼ぶには unsafe が必要
// unsafe = 「Rust のメモリ安全保証をここでは一時的に外す」という宣言
//          中の正しさはプログラマが責任を持つ (C と同じ状況)
let fd = unsafe { libc::open(path, libc::O_RDONLY) };
```

C プログラマにとっては逆方向の利点もあります。
Rust で書いたライブラリを C から呼ぶことも可能なので、
段階的な移行 (C 側は変えずに内部だけ Rust 化) が可能です。

```rust
// Rust の関数を C から呼べるようにエクスポート
#[unsafe(no_mangle)]
pub extern "C" fn sensor_read_raw(handle: *mut Mcp3008, ch: u8) -> i32 { ... }
```

`bindgen` ツールで C ヘッダから Rust バインディングを自動生成できますが、
ポインタ管理は人間が正しさを保証する必要があります。

### 4.6 エコシステムの成熟度

組み込み向けのクレート (ライブラリ) は増えていますが、
Linux 組み込み特有のユーティリティは C/C++ ほど充実していません。

| 用途 | C/C++ | Rust |
|---|---|---|
| SPI/I2C | spidev / i2c-dev (カーネル提供) | 手動 ioctl または `linux-embedded-hal` |
| GPIO | libgpiod | `gpiod` クレート (ラッパー) |
| CAN | socketcan | `socketcan` クレート |
| MQTT | libmosquitto / paho-c | `rumqttc` / `paho-mqtt` |
| SNMP | net-snmp | 成熟したクレートなし |

---

## 5. 移行コストの見積もり

### このプロジェクトの規模感

| コンポーネント | C++ ファイル数 | Rust ファイル数 | 作業時間の目安 |
|---|---|---|---|
| spi-hal | 4 (.hpp/.cpp) | 3 (.rs) | 1〜2 日 |
| i2c-hal | 3 | 2 | 1 日 |
| gpio | 2 | 1 | 0.5〜1 日 |
| libsensor | 4 | 4 | 1〜2 日 |
| libadxl345 | 3 | 3 | 1 日 |
| cli | 1 | 1 | 1 日 |
| テスト全体 | 10+ | 7 | 2〜3 日 |
| CI/CD 整備 | — | — | 1〜2 日 |
| **合計** | **27** | **21** | **約 2〜3 週間** |

> **前提:** Rust 中級者 (所有権・ライフタイムを一人で解決できる) 1 名が担当する場合。
> Rust 未経験者の場合は学習期間を別途 1〜2 ヶ月追加してください。

### 移行後の削減効果

| 項目 | 変化 |
|---|---|
| ビルド設定ファイル | CMakeLists.txt × 7 → Cargo.toml × 7 (総行数 -60%) |
| 静的解析ツール | cppcheck + clang-tidy → cargo clippy (統一) |
| テストフレームワーク | GTest/GMock → cargo test + 手書きモック |
| ASAN/TSAN CI ジョブ | コンパイル時保証のため不要になる (ASAN は残す価値あり) |
| PIMPL ボイラープレート | 削除 |

### 移行しない方が良いケース

- **カーネルモジュール** (`kernel/`)
- **チーム全員が C のみ経験で Rust 学習コストを払えない**
- **リアルタイム要件が厳しく async ランタイムが使えない** (素の `std::thread` で回避可能)
- **ベンダー提供 SDK が C/C++ のみ**

---

## 6. 実際に発見されたバグ

このプロジェクトで C++ → Rust 変換時に発見・修正したバグの一覧です。
いずれも C++ 版では静的解析・レビューをすり抜けていた問題です。

| # | 深刻度 | 内容 | Rust での防止方法 |
|---|---|---|---|
| 1 | Critical | MCP3008 SPI フレームのスタートビット位置ずれ (byte0 と byte1 が逆) | コメント付きプロトコル仕様と一致した実装で再確認 |
| 2 | Critical | GPIO chardev v2 構造体サイズ不一致 (192 vs 592 bytes) → カーネル OOB アクセス | `#[repr(C)]` で正確なサイズを計算・コメントで文書化 |
| 3 | High | I2C `write()` の部分書き込みを無視 → ADS1115 設定が不完全に書かれる | `Write::write_all()` に変更。Rust の型システムで戻り値無視が警告 |
| 4 | High | ADS1115 変換完了ポーリングにタイムアウトなし → ハードウェア故障時に無限ループ | ループ上限を定数 `MAX_POLL_RETRIES` で明示し `Timeout` エラーを返す |
| 5 | High | ADXL345 `open()` で初期化失敗後も `is_open() == true` → 壊れたバスに転送を試みる | 初期化完了後にのみ `self.open = true` をセット (修正パターンがコードに明示) |
| 6 | Medium | GPIO の二重 `request_edge_events()` で fd リーク | `self.close()` を冒頭で呼ぶことで常にクリーンな状態から開始 |
| 7 | Medium | I2C `write_read` が 2 トランザクション (STOP→START) → Repeated Start にならない | `I2C_RDWR` ioctl で 1 アトミックトランザクションに修正 |
| 8 | Low | CLI スレッド停止ロジックが `AtomicBool` と `Mutex<bool>` で二重管理 | `Mutex<bool>` に一本化 |
| 9 | Low | 背景スレッドの `JoinHandle` を保存せず → 出力がフラッシュされずに終了 | `handle.join().ok()` で明示的に完了を待つ |
| 10 | Low | MCP3008 テストが TX バイトを検証していない → Bug #1 が全テスト通過 | `Arc<Mutex<Vec<Vec<u8>>>>` で TX ログを外部から検証可能に |

> **教訓:** Critical/High の 5 件はいずれも「状態管理の一貫性」か「プロトコル仕様の精度」の問題です。
> Rust の型システムや `Result` 強制が直接防げるものは Bug #3〜5 のみで、
> Bug #1・#2 はプロトコル仕様の理解と丁寧なテストが必要です。
> 「Rust にすればバグがゼロになる」ではなく、
> **「特定カテゴリのバグがコンパイル時に排除され、残りのバグに集中できる」** というのが正確な理解です。

---

## 7. 移行戦略

### 推奨: ユーザースペース HAL から段階的に移行

```
Phase 1: spi-hal / i2c-hal (最小 FFI、最高効果)
  └── C++ の ISpiDriver/II2cDriver をトレイトに置き換え
  └── C++ バインディング (cbindgen/cxx) でテスト済み C++ コードと共存

Phase 2: libsensor / libadxl345 (ビジネスロジック)
  └── Phase 1 の Rust ドライバを注入してテスト

Phase 3: cli (CLI ツール)
  └── clap クレートで引数解析、std::thread で背景処理

Phase 4: カーネルモジュール (任意、高難度)
  └── rust-for-linux を使うが、API の安定性に注意
```

### C++ との共存 (cxx クレート)

Rust クレートを C++ から呼ぶには `cxx` クレートが便利です。

```toml
# Cargo.toml
[dependencies]
cxx = "1"
```

```rust
// lib.rs: C++ から呼べる関数をエクスポート
#[cxx::bridge]
mod ffi {
    extern "Rust" {
        type Mcp3008;
        fn new_mcp3008(path: &str, vref: f64) -> Box<Mcp3008>;
        fn read_voltage(self: &mut Mcp3008, ch: u8) -> f64;
    }
}
```

---

## 8. ローカルで試す

### 前提

```sh
# Rust ツールチェーンのインストール (rustup)
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
```

### ビルドとテスト

```sh
cd rust

# ビルド
cargo build

# テスト (実機不要)
cargo test

# Clippy (linter)
cargo clippy -- -D warnings

# ドキュメント生成 (ブラウザで開く)
cargo doc --open
```

### 実機で動かす場合

```sh
# クロスコンパイル例 (ARM Linux)
rustup target add armv7-unknown-linux-gnueabihf
cargo build --target armv7-unknown-linux-gnueabihf --release

# バイナリのサイズ確認
ls -lh target/armv7-unknown-linux-gnueabihf/release/device-ctl
```

---

## 9. 組み込み Rust 完全解説 — Web バックエンドだけではない

> **よくある誤解:** "Rust は Web サーバや CLI ツール向けで、組み込みには使えない"
>
> **実態:** Rust は設計当初から**メモリ安全・ゼロコストが必要な低レイヤ**を主ターゲットとしており、
> Linux カーネル・ bare-metal マイコン・RTOS 上のすべてで動作します。

### 9.1 Rust の動作環境マップ

```
┌─────────────────────────────────────────────────────────────┐
│  Rust が動く場所                                              │
│                                                               │
│  ① Bare-metal (OS なし)                                      │
│     Cortex-M0〜M33 / RISC-V / AVR / Xtensa (ESP32)          │
│     → no_std + 割り込みハンドラ + HAL クレート               │
│                                                               │
│  ② RTOS 上                                                   │
│     FreeRTOS / Zephyr / RTEMS                                 │
│     → embassy (async) または std::thread 相当の API          │
│                                                               │
│  ③ 組み込み Linux (このリポジトリの対象)                      │
│     Raspberry Pi / i.MX8 / AM64x など                        │
│     → std が使える。ioctl / spidev / i2c-dev を直接呼べる    │
│                                                               │
│  ④ Linux カーネル                                            │
│     kernel 6.1 以降で Rust モジュールが正式サポート           │
│                                                               │
│  ⑤ サーバ / デスクトップ / Web (WASM)                        │
│     tokio / axum / wasm-bindgen 等                           │
└─────────────────────────────────────────────────────────────┘
```

---

### 9.2 `std` あり vs `no_std` の違い

| 項目 | `std` あり (組み込み Linux) | `no_std` (bare-metal) |
|------|----|----|
| ヒープ (`Vec`/`Box`) | 使える | `alloc` クレートを追加すれば使える |
| スレッド (`std::thread`) | 使える | 使えない (RTOS タスクで代替) |
| ファイル/ソケット | 使える | 使えない |
| panic ハンドラ | デフォルトで abort | 自前で `#[panic_handler]` が必要 |
| バイナリサイズ | 数百 KB〜 | 数 KB〜 (フラッシュが少ない MCU でも動く) |

**このリポジトリ** は組み込み Linux なので `std` フル利用。
bare-metal へ移植する場合は `std::thread::sleep` を `embassy_time::Timer::after` に
置き換えるだけで大半のコードが再利用できます。

---

### 9.3 bare-metal Cortex-M の具体例

ARM Cortex-M (STM32, nRF52, RP2040 等) での最小構成:

```toml
# Cargo.toml (bare-metal クレート)
[package]
name = "my-sensor-fw"
edition = "2021"

[dependencies]
cortex-m = "0.7"
cortex-m-rt = "0.7"      # スタートアップ + ベクタテーブル
embedded-hal = "1.0"     # SPI/I2C の抽象トレイト
panic-halt = "0.2"       # panic → 無限ループ

[profile.release]
opt-level = "z"          # サイズ最優先
lto = true
```

```rust
#![no_std]
#![no_main]

use cortex_m_rt::entry;
use embedded_hal::spi::SpiBus;

#[entry]
fn main() -> ! {
    // HAL から SPI ペリフェラルを取得
    let peripherals = stm32f4xx_hal::pac::Peripherals::take().unwrap();
    let spi = /* HAL 固有の初期化 */;

    // libsensor の SpiDriver トレイトを embedded-hal の SpiBus に対応させれば
    // read_raw() などをそのまま再利用できる
    loop {
        // センサー読み出し
    }
}
```

C での同等コード (STM32 HAL C):
```c
/* HAL_SPI_TransmitReceive() を直接呼ぶ。型安全なし、エラーチェックは手動 */
HAL_SPI_TransmitReceive(&hspi1, tx, rx, len, HAL_MAX_DELAY);
```

**Rust の優位点:** `embedded-hal` トレイトで SPI の実装を差し替えられるため、
テストは PC 上のモック、実機は STM32 HAL というコードが**同じ型**で書けます。

---

### 9.4 embassy — 組み込み向け async フレームワーク

`embassy` は bare-metal 環境でも `async/await` を使えるフレームワークです。
RTOS の代替として、割り込み駆動の並行処理をコードで表現できます。

```rust
// embassy での SPI センサー読み出し (bare-metal, no RTOS)
#[embassy_executor::task]
async fn sensor_task(mut spi: Spi<'static, SPI1, DMA1_CH3, DMA1_CH2>) {
    loop {
        let mut rx = [0u8; 3];
        spi.transfer(&mut rx, &[0x01, 0x80, 0x00]).await.unwrap();
        let raw = ((rx[1] as u16 & 0x03) << 8) | rx[2] as u16;

        // 次の読み出しまで非同期待機 (CPU は他タスクを実行)
        embassy_time::Timer::after_millis(100).await;
    }
}
```

```c
/* FreeRTOS での同等コード — RTOS API を直接使う必要がある */
void sensor_task(void *pvParameters) {
    for (;;) {
        HAL_SPI_TransmitReceive(&hspi1, tx, rx, 3, HAL_MAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

**embassy の特徴:**
- タスクのスタックは静的確保 — `no_std` でもゼロ動的アロケーション
- `async/await` で並行処理を記述 — 割り込みの複雑な状態機械が不要
- `embassy-time`, `embassy-usb`, `embassy-net` など豊富なドライバ

---

### 9.5 embedded-hal エコシステム

`embedded-hal` は C の「HAL ライブラリ」をトレイトとして定義したクレートです。
**同じドライバが異なるマイコンで動く**のが最大の利点。

```
┌─────────────────────────────────────────────────────────────┐
│  embedded-hal エコシステム                                   │
│                                                               │
│  ┌──────────────┐   使う    ┌──────────────────────────┐    │
│  │ MCP3008 driver│ ──────→ │  SpiDevice (trait)        │    │
│  │ ADS1115 driver│          │  I2c (trait)              │    │
│  └──────────────┘           └──────────────────────────┘    │
│         ↑ 同じコード                ↑ 実装は複数             │
│                          ┌──────────┴──────────┐            │
│                     STM32 HAL           linux-embedded-hal   │
│                     (bare-metal)        (Linux /dev/spidevX) │
└─────────────────────────────────────────────────────────────┘
```

このリポジトリの `SpiDriver` トレイトは `embedded-hal::spi::SpiBus` に近い設計です。
`embedded-hal 1.0` に準拠すればドライバクレートを crates.io で公開・共有できます。

**主要なエコシステムクレート:**

| 用途 | クレート | 説明 |
|------|---------|------|
| SPI/I2C/UART トレイト | `embedded-hal` | ドライバ実装の共通 API |
| Linux 実装 | `linux-embedded-hal` | `/dev/spidevX`, `/dev/i2cY` |
| STM32 実装 | `stm32f4xx-hal` | STM32F4 シリーズ |
| nRF52 実装 | `nrf52840-hal` | Nordic Semiconductor |
| RP2040 実装 | `rp2040-hal` | Raspberry Pi Pico |
| async 対応 | `embedded-hal-async` | embassy 向け |
| テスト用モック | `embedded-hal-mock` | PC 上でユニットテスト |

---

### 9.6 RISC-V サポート

Rust は RISC-V を**ファーストクラス**でサポートしています。

```sh
# ESP32-C3 (RISC-V) 向けターゲットを追加
rustup target add riscv32imc-unknown-none-elf

# SiFive HiFive1 (RISC-V 32bit) 向け
rustup target add riscv32imac-unknown-none-elf

# Linux on RISC-V (64bit)
rustup target add riscv64gc-unknown-linux-gnu
```

C の場合は GCC/Clang に RISC-V ターゲットを別途インストールする必要がありますが、
Rust は `rustup target add` の 1 コマンドで完結します。

---

### 9.7 このリポジトリの位置付け (組み込み Linux)

```
組み込みの深さ                    Rust の対応
─────────────────────────────────────────────────────────
① サーバ / クラウド              ← tokio, axum (Web も)
② デスクトップ / CLI             ← std, clap
③ 組み込み Linux (本リポジトリ)  ← std + ioctl 直呼び  ✓ここ
④ RTOS                           ← embassy + no_std
⑤ bare-metal MCU                 ← cortex-m + no_std
⑥ Linux カーネルモジュール       ← rust-for-linux (6.1+)
─────────────────────────────────────────────────────────
```

このリポジトリは**組み込み Linux** であり、すでに「組み込み」領域にあります。
Rust はここで C/C++ と同等以上のハードウェア制御能力を持ちつつ、
コンパイル時のメモリ安全保証という追加の恩恵を提供します。

---

### 9.8 C との比較まとめ

| 観点 | C | Rust |
|------|---|------|
| ゼロコスト抽象 | ○ | ○ |
| 割り込みハンドラ | ○ | ○ (`#[interrupt]`) |
| DMA / ハードウェアレジスタ | ○ (`volatile`) | ○ (`svd2rust` で型安全) |
| メモリ安全 (コンパイル時) | △ (sanitizer が別途必要) | ○ |
| スタックサイズ | 数百バイト〜 | 数百バイト〜 (同等) |
| バイナリサイズ | 極小可能 | `opt-level = "z"` + LTO で同等 |
| 割り込み安全な共有状態 | 手動 (`volatile`, `__disable_irq`) | `Mutex<CriticalSection>` で型安全 |
| CMSIS / SVD | `*.h` ヘッダ | `svd2rust` で自動生成 |
| デバッグ (GDB/probe-rs) | ○ | ○ (`probe-rs` が GDB 相当) |
| コンパイル速度 | 速い | 遅い (増分ビルドで緩和) |

**結論:** 組み込みでの Rust 採用障壁は「ランタイムの重さ」ではなく「学習コスト」と「エコシステムの成熟度」です。
ハードウェア制御能力は C と同等で、メモリ安全の保証が上乗せされます。

---

## 参考リンク

- [The Embedded Rust Book](https://docs.rust-embedded.org/book/) — 組み込み Rust の公式ガイド
- [Rust for Linux](https://rust-for-linux.com/) — カーネルモジュール開発
- [linux-embedded-hal](https://crates.io/crates/linux-embedded-hal) — embedded-hal の Linux 実装
- [embassy](https://embassy.dev/) — 組み込み向け async フレームワーク
- [cxx](https://cxx.rs/) — C++/Rust 相互運用
- [probe-rs](https://probe.rs/) — Rust 向けデバッグプローブツール (J-Link / CMSIS-DAP 対応)
- [svd2rust](https://docs.rs/svd2rust) — SVD ファイルから型安全なペリフェラル API を生成
- [embedded-hal](https://docs.rs/embedded-hal) — ドライバ共通トレイト定義
- [Awesome Embedded Rust](https://github.com/rust-embedded/awesome-embedded-rust) — 組み込み向けクレート一覧
