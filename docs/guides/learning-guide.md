# 学習ガイド — embedded-device-suite

| 項目 | 内容 |
|---|---|
| 対象読者 | C++組み込みSW開発を学びたいエンジニア |
| 前提知識 | C++の基本文法、Linux/GCCの基本操作 |
| 学習時間目安 | ドキュメント通読: 2〜3時間 / コード通読: 3〜5時間 |

---

## このプロジェクトで学べること

本プロジェクトは、**実案件レベルの組み込みSW開発フロー**を一つのリポジトリで体験できるように設計されています。

```
要件定義 → 基本設計 → 詳細設計 → 実装 → テスト → CI/CD → 納品
```

この流れが、ドキュメント（`docs/`）からコード（`spi-hal/` `libsensor/` `cli/`）、
CI設定（`.github/`）まで一貫して追えます。

---

## ツール別ガイド

本プロジェクトで使われている主要ツールごとに、簡単な説明・構文・本プロジェクトでの
使われ方をまとめた個別ガイドを用意しています。ツールに慣れていない場合は、
このページの C++ 設計パターン解説より先に参照することを推奨します。

| ツール | ガイド |
|---|---|
| ビルドシステム（一括） | [build-guide.md](tooling/build-guide.md) |
| CMake | [cmake-guide.md](tooling/cmake-guide.md) |
| Google Test / GMock | [gtest-guide.md](tooling/gtest-guide.md) |
| Doxygen | [doxygen-guide.md](tooling/doxygen-guide.md) |
| cppcheck / clang-tidy / clang-format | [static-analysis-guide.md](tooling/static-analysis-guide.md) |
| gcov / gcovr（カバレッジ） | [coverage-guide.md](tooling/coverage-guide.md) |
| ASan / UBSan / TSan（サニタイザー）| [sanitizers-guide.md](tooling/sanitizers-guide.md) |
| Linux カーネルモジュール | [kernel-module-guide.md](tooling/kernel-module-guide.md) |
| Docker（再現性あるビルド環境） | [docker-guide.md](tooling/docker-guide.md) |
| Conventional Commits / git-cliff | [commit-conventions-guide.md](tooling/commit-conventions-guide.md) |
| SBOM（成果物管理）| [sbom-guide.md](sbom-guide.md) |

---

## C++設計パターン一覧

### 1. RAII（Resource Acquisition Is Initialization）

**場所**: `spi-hal/src/spi_driver.cpp`

リソース（ファイルディスクリプタ）の取得をコンストラクタで行い、
解放をデストラクタで自動実行する。

```cpp
// spi-hal/src/spi_driver.cpp
SpiDriver::~SpiDriver()
{
    close();   // デストラクタが自動的にファイルをクローズする
}
```

**なぜ重要か**: 例外や早期returnが発生しても確実にリソースが解放される。
組み込みSWでは「ファイルディスクリプタ漏れ」「メモリリーク」は致命的なバグになるため、
RAIIは基本的な防衛策として広く使われる。

**合わせて読む**: [詳細設計書 — SpiDriver](../deliverables/03_detailed-design/spihal-design.md)

---

### 2. PIMPLイディオム（Pointer to IMPLementation）

**場所**: `libsensor/include/sensor.hpp` / `libsensor/src/sensor.cpp`

クラスの実装詳細を内部クラス（`Impl`）に隠蔽し、ヘッダには宣言だけを残す。

```cpp
// libsensor/include/sensor.hpp — ヘッダには Impl の宣言のみ
class Sensor {
    struct Impl;                      // 前方宣言だけ
    std::unique_ptr<Impl> impl_;      // ポインタで保持
};

// libsensor/src/sensor.cpp — 実装の詳細はここだけ
struct Sensor::Impl {
    ISpiDriver* driver;
    bool        owns_driver;
    ...
};
```

**なぜ重要か**:
- ヘッダを変えずに実装を変更できる → **ABI の安定性**
- ヘッダをインクルードしたファイルの**再コンパイルを防げる**
- `#include <linux/spi/spidev.h>` などのプラットフォーム依存ヘッダをライブラリ利用者に露出しない

**合わせて読む**: [詳細設計書 — libsensor](../deliverables/03_detailed-design/libsensor-design.md)

---

### 3. インターフェース分離（Interface Segregation）

**場所**: `spi-hal/include/ispi_driver.hpp`

純粋仮想クラス `ISpiDriver` を定義し、実機実装（`SpiDriver`）と
テスト実装（`MockSpiDriver`）が同一のインターフェースを実装する。

```
ISpiDriver（純粋仮想）
    ├── SpiDriver       — 実機用（/dev/spidev を操作）
    └── MockSpiDriver   — テスト用（GMockで動作を制御）
```

**なぜ重要か**:
- `Sensor` クラスは `ISpiDriver*` しか知らないため、実機なしでテストできる
- 将来のドライバ実装（USB-SPI変換器など）もインターフェースを実装するだけで差し替え可能

---

### 4. 依存注入（Dependency Injection）

**場所**: `libsensor/include/sensor.hpp` — テスト用コンストラクタ

`Sensor` クラスのコンストラクタを2種類用意している:

```cpp
// 実機用: SpiDriver を内部で生成する
explicit Sensor(const std::string& spi_path);

// テスト用: ISpiDriver を外部から差し込む
explicit Sensor(ISpiDriver* driver);
```

テストでは2番目のコンストラクタに `MockSpiDriver` を渡す:

```cpp
// tests/unit/libsensor/test_sensor.cpp
MockSpiDriver mock;
EXPECT_CALL(mock, transfer(_, _, _)).WillOnce(Return(5));

Sensor d(&mock);   // 実機不要でテスト可能
```

**なぜ重要か**:
- テストのために本番コードを変更しなくてよい
- `#ifdef TEST` のような分岐がコードに混入しない
- ユニットテストと結合テストを明確に分離できる

**合わせて読む**: [テスト計画書](../deliverables/06_test/test-plan.md)

---

### 5. `[[nodiscard]]` と `noexcept`

**場所**: `spi-hal/include/ispi_driver.hpp`

```cpp
[[nodiscard]] virtual bool open(const Config& cfg) noexcept = 0;
[[nodiscard]] virtual int  transfer(const uint8_t* tx, uint8_t* rx, size_t len) noexcept = 0;
```

- **`[[nodiscard]]`**: 戻り値を捨てるとコンパイル警告が出る。`open()` の失敗を見落とさないようにする。
- **`noexcept`**: 例外を使わない設計。組み込みSWでは例外を禁止するコーディング規約が多い
  （スタックの制約、リアルタイム性の要求など）。

**なぜ重要か**:
組み込みSWでは「エラーを例外で通知しない」「戻り値で必ず確認する」という規約が一般的。
`[[nodiscard]]` でその確認を**コンパイラに強制させる**のが実用的なテクニック。

---

### 6. ロガーマクロ（syslog + stderr 切替）

**場所**: `spi-hal/include/logger.hpp`

```cpp
// DEBUGビルド: stderrにも出力（開発中のデバッグに便利）
// RELEASEビルド: syslogのみ（デーモンとして動作する場合の標準）
#ifdef DEBUG
#define LOGI(fmt, ...) \
    do { fprintf(stderr, "[INFO] " fmt "\n", ##__VA_ARGS__); \
         syslog(LOG_INFO, fmt, ##__VA_ARGS__); } while(0)
#else
#define LOGI(fmt, ...)  syslog(LOG_INFO, fmt, ##__VA_ARGS__)
#endif
```

**なぜ重要か**:
- `printf` デバッグはリリースビルドでは残してはならない
- `syslog` は Linux デーモン開発の標準的なログ手段（`journalctl -f` で確認できる）
- ビルド設定でログ先を切り替えることで、開発効率と本番での静粛性を両立する

---

### 7. バージョン自動生成（CMake + git tag）

**場所**: `spi-hal/include/version.hpp.in` / `CMakeLists.txt`

```cmake
# CMakeLists.txt: git tag からバージョンを取得
execute_process(
    COMMAND git describe --tags --match "spi-hal/v*" --abbrev=0
    OUTPUT_VARIABLE _tag
)
# version.hpp.in の @EDS_VERSION_SPIHAL@ を置換して version.hpp を生成
configure_file(version.hpp.in version.hpp @ONLY)
```

```cpp
// version.hpp — cmake が自動生成する
namespace embedded::version {
inline constexpr const char* SPIHAL     = "spi-hal/v1.0.1";
inline constexpr const char* GIT_COMMIT = "f92d00f";
}  // 利用側: embedded::version::SPIHAL
```

**なぜ重要か**:
- バイナリ自体がバージョン情報を保持するので、デプロイ後に `device-ctl --version` で確認できる
- CI/CDとタグ管理を連動させることで、出荷物のトレーサビリティが保たれる

**合わせて読む**: [リリースマトリックス](../wiki/release-matrix.md)

---

### 8. Doxygenスニペット（テストをAPIドキュメントに統合）

**場所**: `spi-hal/include/ispi_driver.hpp` + `tests/unit/spi-hal/test_spi_driver.cpp`

```cpp
// ispi_driver.hpp（Doxygenコメント内）
/**
 * **テストケース（UT-DRV-002）** — 無効なパスでは false を返す:
 * @snippet test_spi_driver.cpp UT-DRV-002
 */

// test_spi_driver.cpp（実際のテストコード）
//! [UT-DRV-002]
TEST(SpiDriverOpen, InvalidDeviceReturnsFalse) { ... }
//! [UT-DRV-002]
```

**なぜ重要か**:
- APIドキュメントに**動作するテストコードが引用される**ため、「どう使うか」が伝わりやすい
- テストとドキュメントが同期するため、古くなったサンプルコードの問題が起きにくい

---

### 9. GMock（実機不要のユニットテスト）

**場所**: `tests/mocks/mock_spi_driver.hpp`

```cpp
class MockSpiDriver : public ISpiDriver {
public:
    MOCK_METHOD(bool, open,     (const Config&),                    (noexcept, override));
    MOCK_METHOD(int,  transfer, (const uint8_t*, uint8_t*, size_t), (noexcept, override));
    ...
};
```

テストでは「期待する呼び出し」を事前に設定できる:

```cpp
EXPECT_CALL(mock, transfer(_, _, _)).WillOnce(Return(5));  // 5バイト成功を模倣
```

**なぜ重要か**:
- 実機（Raspberry Pi）がなくてもCIで自動テストできる
- `SpiDriver` が正しく呼ばれているかを `Sensor` の上位から検証できる
- エラーケース（転送失敗、open失敗）のテストが容易になる

---

### 10. AddressSanitizer / ThreadSanitizer（実行時メモリ・競合検出）

**場所**: `CMakeLists.txt`（`ENABLE_SANITIZERS` オプション）/ `.github/workflows/ci.yml`

コンパイラに `-fsanitize=address,undefined`（ASAN+UBSAN）または `-fsanitize=thread`（TSAN）フラグを渡すと、
バイナリにランタイム検査コードが挿入される。静的解析（cppcheck / clang-tidy）が**コードを読んで**問題を探すのに対し、
サニタイザーは**プログラムを実行しながら**問題を検出する。

#### AddressSanitizer + UBSan（ASAN+UBSAN）

**検出対象**: バッファオーバーフロー、use-after-free、未定義動作（整数オーバーフロー等）

```bash
# ローカルで試す（ASAN+UBSAN）
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=asan
cmake --build build-asan -j$(nproc)

cmake -S tests/unit/libsensor -B build-asan/test-lib \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer \
                       -I$(pwd)/libsensor/include -I$(pwd)/tests/mocks" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-asan/test-lib
ASAN_OPTIONS=detect_leaks=1 ./build-asan/test-lib/test_sensor
```

ASANが問題を検出すると **プロセスが即座にアボート** し、スタックトレースを出力する。

#### ThreadSanitizer（TSAN）

**検出対象**: データ競合（複数スレッドが同一メモリを同期なしに読み書きする）

```bash
# ローカルで試す（TSAN）— ASAN とは別ディレクトリ
cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=tsan
cmake --build build-tsan -j$(nproc)

cmake -S tests/unit/libsensor -B build-tsan/test-lib \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer \
                       -I$(pwd)/libsensor/include -I$(pwd)/tests/mocks" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"
cmake --build build-tsan/test-lib
TSAN_OPTIONS=halt_on_error=1 ./build-tsan/test-lib/test_sensor
```

#### このプロジェクトでの学習ポイント: `read_raw_async()` のスレッド危険性

`libsensor/src/sensor.cpp` の `read_raw_async()` を見てほしい:

```cpp
std::thread([this, channel, cb]() {
    auto result = this->read_raw(channel);   // this を生ポインタでキャプチャ！
    ...
}).detach();
```

このコードは `Sensor` オブジェクトのライフタイムが「コールバック完了まで保証される」前提で動作する。
呼び出し側が `Sensor` を先に破棄した場合、use-after-free（ASANが検出）またはデータ競合（TSANが検出）が発生しうる。

ユニットテストでは `condition_variable` で完了を待つため通常は問題なく通るが、
**修正するとすれば** `shared_ptr` + `enable_shared_from_this` を使う:

```cpp
class Sensor : public std::enable_shared_from_this<Sensor> {
    void read_raw_async(uint8_t channel, ReadCallback cb) {
        auto self = shared_from_this();   // shared_ptr でライフタイムを延長
        std::thread([self, channel, cb]() {
            auto result = self->read_raw(channel);
            ...
        }).detach();
    }
};
```

#### ASAN と TSAN を同時に使えない理由

両ツールは異なる「シャドウメモリ」方式でメモリを監視しており、
同一バイナリに混在させるとマッピングが競合してクラッシュする。
CIでは `test:sanitizer:asan` と `test:sanitizer:tsan` を **別ジョブ・別ビルドディレクトリ** に分けることで対処している。

**合わせて読む**: [詳細設計書 — libsensor](../deliverables/03_detailed-design/libsensor-design.md)

---

## CI/CDの仕組み

### GitHub Actions（`.github/workflows/ci.yml`）

```
push/PR → build-and-test → lint（cppcheck）→ docs（Doxygen）→ coverage
```

| ジョブ | 内容 |
|---|---|
| build-and-test | CMakeビルド + ユニットテスト実行 + JUnit XML出力 |
| lint | cppcheck による静的解析 |
| docs | Doxygen HTML 生成 → Artifact 保存 |
| coverage | gcovr でカバレッジ計測 → HTML + Cobertura XML |

---

## ドキュメントと成果物の対応

| ドキュメント | 対応する成果物 |
|---|---|
| 要件定義書 | `docs/deliverables/01_requirements/requirements-spec.md` |
| 基本設計書 | `docs/deliverables/02_basic-design/system-architecture.md` |
| 詳細設計書（spi-hal）| `docs/deliverables/03_detailed-design/spihal-design.md` |
| 詳細設計書（libsensor）| `docs/deliverables/03_detailed-design/libsensor-design.md` |
| API仕様書（SpiDriver）| `docs/deliverables/04_api-spec/spi-driver-api.md` |
| API仕様書（libsensor）| `docs/deliverables/04_api-spec/libsensor-api.md` |
| IF仕様書 | `docs/deliverables/05_interface-spec/spi-hardware-if.md` |
| テスト計画書 | `docs/deliverables/06_test/test-plan.md` |
| リリースノート | `docs/deliverables/07_delivery/release-notes/v1.1.0.md` |

---

## 参考リンク

- [Google Test ドキュメント](https://google.github.io/googletest/)
- [Google Mock ドキュメント](https://google.github.io/googletest/gmock_for_dummies.html)
- [Doxygen マニュアル](https://www.doxygen.nl/manual/)
- [Linux spidev ドキュメント](https://www.kernel.org/doc/html/latest/spi/spidev.html)
