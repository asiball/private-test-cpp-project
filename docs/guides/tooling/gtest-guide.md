# Google Test / Google Mock ガイド

| 項目 | 内容 |
|---|---|
| 対象読者 | C++ のテストフレームワークに不慣れな方 |
| バージョン | Google Test 1.14（libgtest-dev 経由） |
| 本プロジェクトでの使用箇所 | `tests/unit/*`（単体）、`tests/integration/`（結合）、`tests/mocks/` |

---

## Google Test とは

C++ 向けの xUnit 系テストフレームワーク。`TEST` マクロでテストを記述し、
バイナリにリンクして実行すると JUnit 互換 XML が出力できる。

```
test_foo.cpp  →  g++ -lgtest -lgtest_main → ./test_foo
                                              ↓
                                  PASS/FAIL の合計 + XML レポート
```

**Google Mock** は GTest に付属する「モックオブジェクト」フレームワーク。
インターフェースを継承して `MOCK_METHOD` でメソッドを宣言するだけで、
呼出引数 / 回数 / 戻り値を制御できる。

---

## 必須の構文

### 単純なテスト

```cpp
#include <gtest/gtest.h>

TEST(SuiteName, TestName) {
    EXPECT_EQ(1 + 1, 2);              // 失敗時、テスト継続
    ASSERT_TRUE(some_condition());     // 失敗時、即時 return
    EXPECT_NEAR(0.1 + 0.2, 0.3, 1e-9); // 浮動小数の許容差
    EXPECT_FALSE(error_flag);
}
```

`EXPECT_*` は失敗してもテストを継続、`ASSERT_*` はその場で中断（後続コードでクラッシュを避けたい場合）。

### Fixture（前処理・後処理）

```cpp
class MyTest : public ::testing::Test {
protected:
    void SetUp() override    { /* 各テストの前に呼ばれる */ }
    void TearDown() override { /* 各テストの後に呼ばれる */ }
    MyClass obj;
};

TEST_F(MyTest, DoesSomething) {
    EXPECT_TRUE(obj.do_something());
}
```

### スキップ（実機テストなど）

```cpp
TEST(HardwareTest, NeedsRealDevice) {
    if (access("/dev/spidev0.0", F_OK) != 0)
        GTEST_SKIP() << "SPI device not available";
    // …
}
```

### Google Mock

```cpp
#include <gmock/gmock.h>
using ::testing::Return;
using ::testing::_;

class MockSpiDriver : public ISpiDriver {
public:
    MOCK_METHOD(bool, open,     (const Config&),                    (noexcept, override));
    MOCK_METHOD(int,  transfer, (const uint8_t*, uint8_t*, size_t), (noexcept, override));
};

TEST(MyTest, MockUsage) {
    MockSpiDriver mock;
    EXPECT_CALL(mock, open(_)).WillOnce(Return(true));
    EXPECT_CALL(mock, transfer(_, _, _)).WillRepeatedly(Return(3));

    Sensor s(&mock);
    EXPECT_TRUE(s.open());
}
```

| 構文 | 意味 |
|---|---|
| `_` | 「任意の引数」 |
| `Return(x)` | 戻り値を `x` にする |
| `WillOnce(...)`, `WillRepeatedly(...)` | 1 回限り / 何度でも |
| `Times(n)` | 呼出回数の期待値 |
| `DoAll(action1, action2)` | 複数アクションを連結 |

---

## 本プロジェクトでの使われ方

### モック差し込みでテスト可能にする

`Sensor` クラスはコンストラクタで `ISpiDriver*` を受け取る（依存注入）ため、
実機なしで動作確認できる。

```cpp
// tests/mocks/mock_spi_driver.hpp
class MockSpiDriver : public ISpiDriver {
    MOCK_METHOD(...);
};

// tests/unit/libsensor/test_sensor.cpp
TEST(SensorReadRaw, ReturnsTenBitValueViaMock) {
    MockSpiDriver mock;
    EXPECT_CALL(mock, transfer(_, _, 3))
        .WillOnce(DoAll(FillMcp3008Rx(0x2A5), Return(3)));

    Sensor s(&mock);
    auto raw = s.read_raw(0);
    EXPECT_EQ(*raw, 0x2A5);
}
```

### CMake から GTest を組み込む

```cmake
find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(test_sensor test_sensor.cpp)
target_link_libraries(test_sensor
    PRIVATE sensor GTest::GTest GTest::Main GTest::gmock pthread
)

gtest_discover_tests(test_sensor
    XML_OUTPUT_DIR ${CMAKE_BINARY_DIR}/test-results
)
```

`gtest_discover_tests` はバイナリ内のすべての `TEST(...)` を自動列挙して
CTest に登録する（`ctest` で個別実行できる）。

### JUnit XML 出力（CI 用）

```bash
./build/test-libsensor/test_sensor \
    --gtest_output=xml:test-results/libsensor-unit.xml
```

GitHub Actions の `actions/upload-artifact` で `test-results/` を artifact 化し、
失敗時の解析に使う。

### スイート名規約

このプロジェクトでは `TEST(対象クラスOpen, 動作)` のように
**「クラス＋動作カテゴリ」** をスイート名にしている。GTest の `--gtest_filter` で
カテゴリだけ実行できるため:

```bash
./build/test-libsensor/test_sensor --gtest_filter='SensorRead*.*'
```

---

## テストケース ID とコードの紐付け

このプロジェクトでは `test-cases.md` のテーブルに `TEST(Suite, Name)` への
リンクを書き、Markdown の仕様書とコードを 1:1 で対応させている。
詳細は `tests/unit/*/test-cases.md` を参照。

---

## 運用 Tips

- ASSERT_* はスレッド内で使うと挙動が曖昧（早期 return を期待してもスレッド全体は止まらない）。
  スレッドのコールバック内では値を保存して、メインスレッドで EXPECT_* するのが安全
- GMock の未指定呼出は GTest が「想定外」として失敗扱いになる。明示的に許可するには
  `EXPECT_CALL(mock, foo()).Times(::testing::AnyNumber())` を書く
- `--gtest_repeat=N` で N 回繰り返し、`--gtest_shuffle` で順序ランダム化（依存テスト発見）
- 実行時間が長いテストには `EXPECT_*` を冗長に並べず、Fixture で初期化を共通化する
