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

## ドキュメントと成果物の対応

本プロジェクトでは、各開発フェーズの成果物が `docs/` 以下に配置されています。開発フローに沿ってドキュメントを読み進めることで、実案件に近い体験が可能です。

| 開発フェーズ | ドキュメント | 概要・対応する成果物 |
|---|---|---|
| 1. 要件定義 | [要件定義書](../deliverables/01_requirements/requirements-spec.md) | システム全体の目的、機能要件、非機能要件 |
| 2. 基本設計 | [基本設計書](../deliverables/02_basic-design/system-architecture.md) | システムの全体構成、コンポーネント配置、ビルド・デプロイ設計 |
| 3. 詳細設計 | [詳細設計書（spi-hal）](../deliverables/03_detailed-design/spihal-design.md) | SPIドライバコンポーネントのクラス設計、状態遷移 |
| | [詳細設計書（libsensor）](../deliverables/03_detailed-design/libsensor-design.md) | センサー制御ライブラリのクラス設計、スレッドモデル |
| 4. API設計 | [API仕様書（SpiDriver）](../deliverables/04_api-spec/spi-driver-api.md) | SPIドライバが公開する関数・引数の詳細 |
| | [API仕様書（libsensor）](../deliverables/04_api-spec/libsensor-api.md) | センサー制御APIおよびコールバック仕様 |
| 5. インターフェース設計 | [IF仕様書](../deliverables/05_interface-spec/spi-hardware-if.md) | SPI通信の物理配線およびメッセージのプロトコル仕様 |
| 6. テスト設計 | [テスト計画書](../deliverables/06_test/test-plan.md) | テスト手法、UT/IT/STの範囲、カバレッジ目標 |
| 7. 納品・リリース | [リリースノート](../deliverables/07_delivery/release-notes/v1.1.0.md) | バージョンごとの更新履歴、リリース手順 |

> フェーズ番号はディレクトリ構成（`04_api-spec/` → `05_interface-spec/`）と一致させている。
> README.md の「開発ライフサイクル」図と同じ並びなので、どちらから読んでも齟齬はない。

> 上表は学習用に主要コンポーネント（SpiDriver / libsensor）を抜粋したもの。
> **全コンポーネント（i2c-hal / gpio / libadxl345 / ADS1115 / device-ctl / kernel / logger）の
> 詳細設計・API・IF 仕様の対応表は、[納品ドキュメント索引](../deliverables/README.md) を参照。**

---

## コードを読む場合のお勧め順序

コードの関連性を深く理解するために、以下の順序で読み進めることを推奨します。

1. **[spi-hal/include/ispi_driver.hpp](../../spi-hal/include/ispi_driver.hpp)（インターフェース設計）**
   - まずはドライバの抽象的なインターフェースを確認します。C++の純粋仮想関数の使い方や例外を排除した `noexcept` 設計を学べます。
2. **[spi-hal/include/spi_driver.hpp](../../spi-hal/include/spi_driver.hpp) / [spi-hal/src/spi_driver.cpp](../../spi-hal/src/spi_driver.cpp)（実機ドライバ実装）**
   - Linux の `/dev/spidev` を操作する具体的な実装です。ファイルオープンから設定、データ転送までの基本的なシステムプログラミング、およびデストラクタでリソースを確実に解放する **RAII** パターンが学べます。
3. **[libsensor/include/sensor.hpp](../../libsensor/include/sensor.hpp) / [libsensor/src/sensor.cpp](../../libsensor/src/sensor.cpp)（PIMPL + 非同期API）**
   - ドライバをラップしてセンサーの物理値を制御するライブラリです。ヘッダに実装詳細を見せない **PIMPL** パターンや、スレッドをバックグラウンドで走らせる非同期API設計が学べます。
4. **[tests/mocks/mock_spi_driver.hpp](../../tests/mocks/mock_spi_driver.hpp)（モック実装）**
   - ユニットテストにおいて、実機のSPIドライバなしで動作をテストするために **Google Mock** を使用したモックの定義方法を学べます。
5. **[tests/unit/](../../tests/unit/)（ユニットテスト）**
   - ドライバやライブラリの動作を担保するためのテストコードです。インターフェースを差し替えてモックで動作確認を行う **依存注入 (DI)** パターンの実践を学べます。

ここまでで「SPI + コマンド型 ADC + PIMPL/DI」という 1 本の縦串を読み切れます。
さらに視野を広げたい場合は、**同じ設計の枠組みが別の題材でどう引き直されるか**を以下で読み比べてください（いずれも任意コンポーネント）。

6. **[libadxl345/include/adxl345.hpp](../../libadxl345/include/adxl345.hpp) / [libadxl345/src/adxl345.cpp](../../libadxl345/src/adxl345.cpp)（レジスタ型デバイス）**
   - MCP3008 の「コマンド型」に対し、ADXL345 は **レジスタ型**。`read_reg`/`write_reg`/`update_bits`（read-modify-write）でビット単位に設定する例です。レジスタマップ（[adxl345-register-map.md](../deliverables/05_interface-spec/adxl345-register-map.md)）とコードの 1:1 対応を確認できます。
7. **[gpio/include/gpio_line.hpp](../../gpio/include/gpio_line.hpp) / [gpio/src/gpio_line.cpp](../../gpio/src/gpio_line.cpp)（割り込み駆動）**
   - 「ポーリングして待つ」代わりに **epoll でイベントを待つ** 設計です。組み込みらしいイベント駆動の入口として [GPIO 割り込みと epoll](gpio-interrupts-epoll.md) と合わせて読みます。
8. **[i2c-hal/include/ii2c_driver.hpp](../../i2c-hal/include/ii2c_driver.hpp) / [libsensor/include/ads1115.hpp](../../libsensor/include/ads1115.hpp)（別バスへの引き直し）**
   - SPI とは別の **I2C バス**で同じ「ADC を読む」仕事をする例です。`ISpiDriver` に対する `II2cDriver`、`Sensor` に対する `Ads1115` という対応で、抽象化の枠組みが再利用される様子が分かります（[I2C と ADS1115](i2c-and-ads1115.md)）。

> 全コンポーネントの詳細設計・API・IF 仕様の対応表は [納品ドキュメント索引 §2](../deliverables/README.md#2-コンポーネント別-対応表ナビ兼整備状況) にあります。

---

## C 開発者の方へ（C++ への導入）

組み込みで C 中心に書いてきた方が C++ へ一歩踏み出すための導入資料です。
「いつもの C のイディオムが C++ ではどうなるか」という対比で読めます。

- [C→C++ ステップアップガイド](c-to-cpp-stepping-stones.md)
  — 不透明ポインタ→PIMPL、ops 表→インターフェース、`goto err`→RAII、`#define`→`enum class`、`-1`→`std::optional`
- [I2C と ADS1115](i2c-and-ads1115.md) — SPI との違いとバス抽象の引き直し
- [GPIO 割り込みと epoll](gpio-interrupts-epoll.md) — ポーリング vs イベント駆動

---

## ツール別ガイド

本プロジェクトで使用されている主要なツールや技術要素について、概要と本プロジェクトでの用途をまとめています。

| ツール・テーマ | ガイドリンク | 主な役割 |
|---|---|---|
| ビルドシステム | [build-guide.md](tooling/build-guide.md) | Dockerおよびローカルでのビルド手順、CI/CDとの関係 |
| C++ 設計パターン | [cpp-patterns-guide.md](tooling/cpp-patterns-guide.md) | RAII、PIMPL、DI、GMock、ASanなどの設計パターン解説 |
| CMake | [cmake-guide.md](tooling/cmake-guide.md) | プロジェクトのビルド定義、依存関係管理 |
| Google Test / GMock | [gtest-guide.md](tooling/gtest-guide.md) | 単体テストフレームワークとモックの使用方法 |
| Doxygen | [doxygen-guide.md](tooling/doxygen-guide.md) | APIドキュメントの自動生成とコメント規約 |
| 静的解析 | [static-analysis-guide.md](tooling/static-analysis-guide.md) | cppcheck / clang-tidy / clang-format によるコード品質維持 |
| カバレッジ | [coverage-guide.md](tooling/coverage-guide.md) | gcov / gcovr を用いたテスト網羅率の計測方法 |
| サニタイザー | [sanitizers-guide.md](tooling/sanitizers-guide.md) | ASan / UBSan / TSan によるメモリリークや競合状態の検出 |
| Linux カーネルモジュール | [kernel-module-guide.md](tooling/kernel-module-guide.md) | 本格的な組み込み開発のためのカスタムドライバ構築 |
| Docker | [docker-guide.md](tooling/docker-guide.md) | 再現性のあるビルド環境の構築方法 |
| コミット規約 | [commit-conventions-guide.md](tooling/commit-conventions-guide.md) | Conventional Commits と git-cliff による自動変更履歴生成 |
| SBOM | [sbom-guide.md](sbom-guide.md) | ソフトウェア部品表によるサプライチェーンセキュリティ対策 |

---

## 主なC++設計パターンの要約

本プロジェクトで活用されている主要なC++の設計・コーディング手法の概要です。詳細は [C++設計パターンガイド](tooling/cpp-patterns-guide.md) を参照してください。

- **RAII**: デストラクタを利用してファイルなどのシステムリソースの解放漏れを防ぎます。
- **PIMPLイディオム**: ヘッダファイルに実装詳細を露出させないことで、コンパイル依存関係を最小限に抑え、ABIを安定させます。
- **インターフェース分離 & 依存注入 (DI)**: 純粋仮想クラスによるインターフェース定義と外部からの差し込み（DI）により、実機不要でテスト可能なコードを実現します。
- **Google Mock (GMock)**: 呼び出し回数や戻り値の制御を宣言的に記述し、複雑な条件下のテストを容易にします。
- **`[[nodiscard]]` と `noexcept`**: 戻り値チェックのコンパイル強制と、例外を使わない安全なエラーハンドリング。
- **AddressSanitizer (ASan) / ThreadSanitizer (TSan)**: 実行時のメモリバグやスレッド間データ競合を検出する強力な診断ツール。

---

## CI/CDの仕組み

本プロジェクトでは、コードの変更ごとに自動的にテストや検証を行うためのCI/CDパイプラインを構築しています。

GitHub Actions（`.github/workflows/ci.yml`）により、以下の工程が自動実行されます：
1. **CMakeビルド ＆ ユニットテスト**の実行
2. **静的解析（cppcheck）**によるコード品質チェック
3. **Doxygenによるドキュメント生成**
4. **gcovrによるテストカバレッジ計測**

パイプラインの構成や各タスクの詳細については、[ビルドガイドの第5節「CI/CDパイプライン」](tooling/build-guide.md#5-cicd%E3%83%91%E3%82%A4%E3%83%97%E3%83%A9%E3%82%A4%E3%83%B3) を参照してください。

---

## 参考リンク

- [Google Test ドキュメント](https://google.github.io/googletest/)
- [Google Mock ドキュメント](https://google.github.io/googletest/gmock_for_dummies.html)
- [Doxygen マニュアル](https://www.doxygen.nl/manual/)
- [Linux spidev ドキュメント](https://www.kernel.org/doc/html/latest/spi/spidev.html)
