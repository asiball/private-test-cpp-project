# サニタイザーガイド（ASan / UBSan / TSan）

| 項目 | 内容 |
|---|---|
| 対象読者 | メモリ破壊・データ競合を実行時に検出したい方 |
| 取り上げるツール | AddressSanitizer (ASan)、UndefinedBehaviorSanitizer (UBSan)、ThreadSanitizer (TSan) |
| 本プロジェクトでの使用箇所 | ルート `CMakeLists.txt` の `ENABLE_SANITIZERS`、`.github/workflows/ci.yml` の `sanitizer` ジョブ |

---

## サニタイザーとは

コンパイラに `-fsanitize=<種類>` を渡すと、バイナリに **ランタイム検査コード** が
挿入される。静的解析（cppcheck / clang-tidy）が「コードを読んで」問題を探すのに対し、
サニタイザーは「プログラムを実行しながら」問題を検出する。

| サニタイザー | 検出対象 | 性能オーバーヘッド |
|---|---|---|
| **ASan + UBSan** | バッファオーバーフロー、use-after-free、未初期化メモリ参照、未定義動作（整数オーバーフロー等） | 2〜3 倍程度 |
| **TSan** | 複数スレッド間のデータ競合 | 5〜15 倍 |

ASan と TSan は **同時使用不可**。別ビルドディレクトリで実行する。

---

## 構文の基本

### ASan + UBSan ビルド

```bash
cmake -S . -B build-asan \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-asan -j$(nproc)

ASAN_OPTIONS=detect_leaks=1 ./build-asan/test-libsensor/test_sensor
```

`-fno-omit-frame-pointer` は **スタックトレースを取得するため**に必須。
`detect_leaks=1` でメモリリーク検出を有効化。

### TSan ビルド

```bash
cmake -S . -B build-tsan \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"
cmake --build build-tsan -j$(nproc)

TSAN_OPTIONS=halt_on_error=1 ./build-tsan/test-libsensor/test_sensor
```

### 検出時の挙動

サニタイザーが問題を検出すると、**プロセスが即座にアボート** し、
スタックトレースを stderr に出力する。CI ジョブでは終了コード != 0 で fail する。

---

## 本プロジェクトでの使われ方

### ルート `CMakeLists.txt` のオプション

```cmake
set(ENABLE_SANITIZERS "none" CACHE STRING "none | asan | tsan")
set_property(CACHE ENABLE_SANITIZERS PROPERTY STRINGS none asan tsan)

if(ENABLE_SANITIZERS STREQUAL "asan")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
elseif(ENABLE_SANITIZERS STREQUAL "tsan")
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
endif()
```

ローカルで試すなら:

```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=asan
cmake --build build-asan -j$(nproc)
ASAN_OPTIONS=detect_leaks=1 ./build-asan/cli/device-ctl --version
```

### CI（`sanitizer` ジョブ）

`.github/workflows/ci.yml` の `sanitizer` ジョブが ASan/UBSan と TSan を両方走らせる。
TSan は特に `Sensor::read_raw_async()` のスレッド安全性を検査する役割。

```yaml
- name: Libsensor unit tests (TSAN) — read_raw_async スレッド検査
  run: |
    cmake -S libsensor -B build/libsensor-tsan \
      -DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer"
    cmake --build build/libsensor-tsan -j$(nproc)
    TSAN_OPTIONS=halt_on_error=1 ./build/test-libsensor-tsan/test_sensor
```

---

## このプロジェクトでの学習ポイント — `read_raw_async()` のスレッド危険性

本プロジェクトの `Sensor::read_raw_async()` には、コールバック完了前に `Sensor` オブジェクトが破棄されると解放済みメモリを参照しうるライフタイムの問題（スレッド危険性）が内在しています。

- **ASan**: コールバック実行時に use-after-free としてバグを検出します。
- **TSan**: 破棄スレッドとコールバックスレッド間のデータ競合として同期不足を検出します。

このような非自明な並行バグが、人手のテストでは再現しにくくても、サニタイザーは決定論的に検出してくれます。
この問題を解決するためのC++の具体的な設計パターン（`shared_ptr` と `enable_shared_from_this` によるライフタイム延長）については、[C++設計パターンガイド — AddressSanitizer / ThreadSanitizer](../tooling/cpp-patterns-guide.md#10-addresssanitizer--threadsanitizer%E5%AE%9F%E8%A1%8C%E6%99%82%E3%83%A1%E3%83%A2%E3%83%AA%E7%AB%B6%E5%90%88%E6%A4%9C%E5%87%BA) を参照してください。

---

## 運用 Tips

- ASan + TSan を **同時に有効化はできない**。CI では別ジョブで両方走らせる
- ASan は **ヒープオーバーフロー** には強いが、**スタックオーバーフロー** はカバー範囲外
- `--coverage` フラグと **同時使用不可**。カバレッジ計測は別ビルドで
- 「false positive」は GMock / GTest のような重量ライブラリで時々発生する。
  `LSAN_OPTIONS=suppressions=lsan.supp` で抑制可能
- 速度オーバーヘッドが大きいため、結合テストや受け入れテストで毎回走らせるのは非実用的。
  単体テストと「クリティカルパス」のみが対象になる
