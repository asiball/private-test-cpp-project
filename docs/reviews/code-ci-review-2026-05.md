# 第三者コード / CI レビュー要約 (2026-05)

`embedded-device-suite` を第三者視点で再確認した結果をまとめる。プロジェクト全体
(CMake ビルド、Google Test/Mock によるテスト、cppcheck/clang-tidy、サニタイザー、
Doxygen、SBOM、リリース自動化)は良く整備されている。本レビューでは、リスクの低い
修正のみを適用し、それ以外の指摘は推奨事項として記録する方針とした。

## 本タスクで修正した項目

| # | 種別 | ファイル | 内容 |
|---|------|----------|------|
| 1 | バグ修正 | `spi-hal/src/kernel_spi_driver.cpp` | `KernelSpiDriver::transfer()` に `len > UINT32_MAX` のオーバーフローガードを追加。`SpiDriver::transfer()`(`spi-hal/src/spi_driver.cpp:76-80`)に存在していたガードが欠落しており、`size_t len` を直接 `uint32_t` へキャスト(切り捨て)していた不整合を解消。検証順序を `SpiDriver` と揃えた(fd → null → len==0 → overflow)。 |
| 2 | テスト追加 | `tests/unit/spi-hal/test_kernel_spi_driver.cpp` | UT-KDRV-011 を追加。オーバーフローパスは `fd_ >= 0` を要するため、既存テスト(UT-KDRV-008/009)と同様に実機がない環境では `GTEST_SKIP()` する形とした。 |
| 3 | 可読性改善 | `libsensor/src/sensor.cpp` | MCP3008 プロトコルのマジックナンバー(`0x01`/`0x80`/`0x03`)を無名 namespace の名前付き定数(`MCP3008_START_BIT` / `MCP3008_SINGLE_MODE` / `MCP3008_RESULT_MASK`)に置換。公開ヘッダ・ABI は不変。値・動作も不変。 |

## 未対応(意図的な設計判断)

- **`Sensor::read_raw_async()` の use-after-free リスク**(`libsensor/src/sensor.cpp:88-92`):
  `std::thread` を `this` キャプチャで生成し `detach()` している。Sensor オブジェクトが
  コールバック完了前に破棄されると未定義動作になる。ただしヘッダ(`sensor.hpp:126`)・
  実装の双方のコメントで「ライフタイムは呼び出し側が保証する」と明記された設計判断であり、
  根本対処は API 変更(`std::shared_ptr` + `enable_shared_from_this` あるいは `std::jthread`
  による join 可能化)を伴うため、本タスクの範囲外とした。

## 未対応(将来の改善候補)

- **所有権管理**: `Sensor::Impl` が生ポインタ `ISpiDriver* driver` と手動の `owns_driver`
  フラグで所有権を管理している(`libsensor/src/sensor.cpp:10-25`)。
  `std::unique_ptr<ISpiDriver>`(所有時)+ 非所有参照の区別へ移行すると安全性が上がる。
- **重複ロジックの共通化**: `SpiDriver::transfer()` と `KernelSpiDriver::transfer()` は
  fd チェック / null チェック / len==0 / オーバーフローの検証ロジックがほぼ同一。
  共通ヘルパへの抽出を検討。
- **`strerror()` のスレッド安全性**: 各ドライバのエラーログで `strerror()` を使用している
  (`spi_driver.cpp` / `kernel_spi_driver.cpp`)。CLI の監視スレッド(`cli/src/main.cpp`)と
  併用される文脈では `strerror_r()` の利用が望ましい。
- **EAGAIN リトライ**: `SpiDriver::transfer()` のリトライ(`spi_driver.cpp:91-95`)は
  固定 3 回・バックオフ無し。`std::this_thread::sleep_for` による待機やバックオフを検討。

## CI に関する指摘(今回は現状維持・推奨のみ)

1. **`test_kernel_spi_driver` が CI で実行されていない**:
   `.github/workflows/ci.yml` の "SPI HAL unit tests" ステップは
   `./build/test-spihal/test_spi_driver` のみを実行している。CMake では
   `test_kernel_spi_driver` もビルドされる(`tests/unit/spi-hal/CMakeLists.txt:19`)が、
   バイナリが起動されないため UT-KDRV-* が一切評価されない。
   `ctest` 経由(`gtest_discover_tests` 済み)に切り替えるか、当該バイナリも明示実行することを推奨。
2. **GTest のソースビルドの重複**: `build-and-test` / `coverage` / `sanitizer` の各ジョブで
   `/usr/src/googletest` からのビルド+インストール手順が重複している。composite action 化、
   あるいはビルド成果物のキャッシュ/共有でジョブ時間を削減できる。

## 検証

- リリースビルド・libsensor 単体テスト・spi-hal テストのビルド/実行、cppcheck を実施。
  詳細手順は本リポジトリの CI(`.github/workflows/ci.yml`)および
  プラン(検証セクション)を参照。
