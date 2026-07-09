# CMake ガイド

| 項目 | 内容 |
|---|---|
| 対象読者 | C++ ビルドの経験はあるが CMake に慣れていない方 |
| バージョン | CMake 3.10 以上 |
| 本プロジェクトでの使用箇所 | ルート `CMakeLists.txt` + 各サブディレクトリの `CMakeLists.txt` |

---

## CMake とは

C/C++ のビルド「設定」を記述するためのメタビルドツール。`CMakeLists.txt` を読み込んで
プラットフォームごとのビルドファイル（`Makefile` / Ninja / Visual Studio など）を生成する。

```
CMakeLists.txt  →  cmake -B build  →  Makefile / build.ninja / *.vcxproj
                                         ↓
                                     cmake --build build
```

利点:
- ヘッダ依存関係を自動追跡してくれる
- 複数プラットフォーム対応（同じ記述で Linux / macOS / Windows）
- 外部ライブラリ検索（`find_package`）
- 「サブディレクトリの CMakeLists を取り込む」モノレポ構成に強い

---

## 必須の構文

### プロジェクト宣言とターゲット

```cmake
cmake_minimum_required(VERSION 3.10)
project(my-app VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(my-app src/main.cpp)
add_library(my-lib STATIC src/lib.cpp)
add_library(my-shared SHARED src/lib.cpp)

# ヘッダ検索パスを公開
target_include_directories(my-lib PUBLIC include)

# リンク
target_link_libraries(my-app PRIVATE my-lib pthread)
```

| キーワード | 意味 |
|---|---|
| `PUBLIC`  | 自分も使う & このターゲットを使う側も使う |
| `PRIVATE` | 自分だけ使う |
| `INTERFACE` | 自分は使わない & このターゲットを使う側だけ使う |

### 設定変数とオプション

```cmake
# キャッシュ変数（cmake -DENABLE_FOO=ON で上書き可能）
set(ENABLE_FOO ON CACHE BOOL "Foo を有効化")

# 条件分岐
if(ENABLE_FOO)
    target_compile_definitions(my-lib PRIVATE FOO_ENABLED)
endif()
```

### configure_file（テンプレートからファイル生成）

```cmake
configure_file(
    ${CMAKE_SOURCE_DIR}/version.hpp.in
    ${CMAKE_SOURCE_DIR}/version.hpp
    @ONLY   # @VAR@ のみ置換、${VAR} は無視
)
```

`version.hpp.in` 側:

```cpp
inline constexpr const char* APP_VERSION = "@PROJECT_VERSION@";
```

### サブディレクトリ取り込み

```cmake
add_subdirectory(lib)   # lib/CMakeLists.txt を読み込み、そのターゲットを公開
add_subdirectory(cli)
```

---

## 本プロジェクトでの使われ方

モノレポ構成で、ルートと各サブディレクトリに `CMakeLists.txt` が分散している。
トップの `foreach(_component ...)` が「存在するものだけ」を依存順に取り込むため、
★任意コンポーネントはディレクトリごと削除しても残りはビルドできる（→ ADR 0001）。

```
CMakeLists.txt               ← トップレベル（共通設定 + サブディレクトリ取り込み）
├── spi-hal/CMakeLists.txt   ← 静的ライブラリ libspihal.a
├── i2c-hal/CMakeLists.txt   ← 静的ライブラリ libi2chal.a   ★任意
├── gpio/CMakeLists.txt      ← 静的ライブラリ libgpio.a     ★任意
├── libsensor/CMakeLists.txt ← 共有ライブラリ libsensor.so（+ libads1115.so ★任意）
├── libadxl345/CMakeLists.txt← 共有ライブラリ libadxl345.so
├── libmcp9808/CMakeLists.txt← 共有ライブラリ libmcp9808.so ★任意
├── cli/CMakeLists.txt       ← 実行バイナリ device-ctl
└── examples/CMakeLists.txt  ← サンプル（ads1115_alert_demo ほか計 5 バイナリ）★任意
```

> 以降の「工夫ポイント」は `libsensor`(SPI 系) を代表例に説明するが、`i2c-hal` / `gpio` /
> `libadxl345` も**同じ仕組み**（`if(TARGET ...)` でモノレポ/スタンドアロン両対応、
> コンポーネント別バージョンタグ）を踏襲している。

### 工夫ポイント

**1. git describe からバージョン自動取得**（ルート `CMakeLists.txt`）

```cmake
execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags --match "spi-hal/v*" --abbrev=0
    OUTPUT_VARIABLE EDS_VERSION_SPIHAL
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
configure_file(spi-hal/include/version.hpp.in spi-hal/include/version.hpp @ONLY)
```

`device-ctl --version` がコンパイル時に埋め込まれたタグ名を表示できる。

**2. モノレポビルド と スタンドアロンビルドの両対応**（`libsensor/CMakeLists.txt`）

```cmake
if(TARGET spihal)
    # モノレポビルド: spihal を PUBLIC リンク。sensor.hpp が ispi_driver.hpp を
    # include するため、include 要件は INTERFACE（公開）。消費側は find_dependency(spihal)
    # 経由で解決する。
    target_link_libraries(sensor PUBLIC spihal)
    target_link_libraries(sensor PRIVATE pthread)
else()
    # スタンドアロンビルド (cmake -S libsensor/) の場合は spi-hal のソースを直接コンパイル
    target_sources(sensor PRIVATE ../spi-hal/src/spi_driver.cpp)
endif()
```

依存 HAL（`spihal`）を `PRIVATE` ではなく **`PUBLIC`** でリンクしているのが要点（issue #47 /
[ADR 0002](../../adr/0002-installed-header-self-containment.md)）。`sensor.hpp` が `ispi_driver.hpp` を
単純名で include するため、`find_package(sensor)` した利用側にも spi-hal の include パスと
リンクが伝播する必要があり、`PUBLIC` にしないと利用側で `ispi_driver.hpp` が見つからない。
`pthread` は `sensor.cpp` 内部でしか使わないため `PRIVATE` のままでよい。

これにより `cmake -S libsensor -B build/libsensor` だけでもライブラリ単体ビルドが可能。

**3. サニタイザーフラグの切替**（ルート `CMakeLists.txt`）

```cmake
set(ENABLE_SANITIZERS "none" CACHE STRING "none | asan | tsan")
if(ENABLE_SANITIZERS STREQUAL "asan")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()
```

---

## よくある操作のクックブック

```bash
# Release ビルド
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Debug ビルド（別ディレクトリにすると Release と共存できる）
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug

# 単一ターゲットだけビルド
cmake --build build --target spihal

# インストール（CMakeLists の install(TARGETS ...) で指定した場所へ）
cmake --install build --prefix /usr/local

# キャッシュをクリアしてやり直す
rm -rf build
```

---

## 運用 Tips

- `build/` を **必ず .gitignore** に入れる
- `CMakeLists.txt` 変更時は `cmake -B build` を再実行（`--build` だけでは反映されない場合がある）
- `compile_commands.json`（clang-tidy 等が使う）は `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` で生成
- 行儀のよい CMake は「`if(NOT TARGET ...)` で重複定義を防ぐ」「`target_*` を使う（`add_definitions` ではなく）」
