# SBOM 管理ガイド — embedded-device-suite

| 項目 | 内容 |
|---|---|
| ドキュメント番号 | DEL-SBOM-001 |
| バージョン | 1.0 |
| 作成日 | 2026-05-26 |

---

## 1. このドキュメントの目的

本プロジェクトでは `sbom.spdx`（SPDX 2.3）と `sbom.cdx.json`（CycloneDX 1.6）の 2 ファイルを  
リポジトリルートに配置し、`tools/generate-sbom.py` で自動生成している。

このドキュメントは以下を記録する。

- SBOM ツールの選定理由（なぜ syft を使わなかったか）
- 収録パッケージとその選定基準
- Linux 標準ライブラリの扱い方と判断基準
- SBOM を更新すべきタイミングと手順
- 今後パッケージを追加・削除する際の判断基準

---

## 2. ツール選定：なぜ syft を使わなかったか

### 2.1 syft とは

[syft](https://github.com/anchore/syft)（Anchore 社）は SBOM 生成のデファクトスタンダードツールであり、  
SPDX・CycloneDX の両形式に対応し、Docker イメージ・ディレクトリ・コンテナを対象にスキャンできる。

### 2.2 syft が有効なケース

| ケース | 説明 |
|---|---|
| npm / pip / Maven / Cargo | lock ファイルから依存グラフを正確に自動構築できる |
| コンテナイメージのスキャン | インストール済みの deb/rpm パッケージを検出できる |
| Go modules | `go.sum` から再現性ある依存解決が可能 |

### 2.3 本プロジェクトで syft を使わなかった理由

本プロジェクトは C++17 + CMake 構成であり、パッケージマネージャファイルが存在しない。

```
使用していないもの: conanfile.txt, vcpkg.json, requirements.txt, package.json
```

このため syft をディレクトリスキャンしても **CMakeLists.txt の `find_package()` 宣言は検出されない**。  
たとえば以下の依存は syft では取得できない。

| 依存 | CMakeLists.txt での宣言 | syft での検出 |
|---|---|---|
| googletest 1.14.0 | `find_package(GTest REQUIRED)` | **不可** |
| linux-spi-headers | `#include <linux/spi/spidev.h>` | **不可** |
| glibc (syslog, pthread) | ソースコードの `#include` のみ | **不可** |

syft をベースにした場合でも、不足分を補完する手動メタデータファイルが必要になる。  
補完処理を加えると syft を使う利点（自動化・精度）が消え、構成が複雑になるだけとなる。

### 2.4 採用したアプローチ：Python スクラッチ生成

`tools/sbom-metadata.json` を**依存関係の単一ソース（Single Source of Truth）**とし、  
`tools/generate-sbom.py` が SPDX 2.3 / CycloneDX 1.6 の両形式を生成する。

```
tools/sbom-metadata.json   ← 手動管理・依存関係の定義
        │
        └─ tools/generate-sbom.py → sbom.spdx
                                  → sbom.cdx.json
```

**メリット**

- 外部ツール不要（Python 3.6 stdlib のみ）
- ローカル実行・CI 実行で完全に同一の出力
- CMakeLists.txt の `find_package()` や `#include` で明示されない依存も正確に記載できる
- ライセンス表現（`GPL-2.0-only WITH Linux-syscall-note` 等）を完全制御できる

**デメリット（＝ 将来の注意点）**

- 依存が追加・変更されたとき、`sbom-metadata.json` の手動更新が必要
- syft のようなスキャンによる「うっかり漏れ」防止機能はない

> **syft を将来導入すべきケース**：conan や vcpkg を採用してサードパーティライブラリが  
> 増えた場合は syft Docker イメージ（`anchore/syft:latest`）の導入を再検討する。  
> その場合も `sbom-metadata.json` の補完情報は引き続き必要になる。

---

## 3. 収録パッケージと選定基準

### 3.1 現在の収録パッケージ一覧（9 パッケージ）

| パッケージ名 | 種別 | バージョン | ライセンス | 理由 |
|---|---|---|---|---|
| embedded-device-suite | 自プロジェクト（ルート） | 1.1.0 | MIT | SBOM の対象プロダクト |
| spi-driver | 自プロジェクト（静的ライブラリ） | 1.1.0 | MIT | 内部コンポーネント |
| my-spi-driver | 自プロジェクト（カーネルモジュール） | 1.1.0 | GPL-2.0-only | 内部コンポーネント |
| libsensor | 自プロジェクト（共有ライブラリ） | 1.1.0 | MIT | 内部コンポーネント |
| device-ctl | 自プロジェクト（実行ファイル） | 1.1.0 | MIT | 内部コンポーネント |
| googletest | 外部ライブラリ（テストのみ） | 1.14.0 | BSD-3-Clause | テスト依存。`TEST_DEPENDENCY_OF` で明示 |
| glibc | システムライブラリ | NOASSERTION | LGPL-2.1-or-later | POSIX インタフェース群（下記 §3.2 参照） |
| linux-kernel-headers | システムヘッダ | NOASSERTION | GPL-2.0-only WITH Linux-syscall-note | カーネル・UAPI ヘッダ（下記 §3.3 参照） |
| libstdc++ | システムライブラリ | NOASSERTION | GPL-3.0-only WITH GCC-exception-3.1 | C++17 ランタイム（下記 §3.4 参照） |

### 3.2 包含・除外の基準

SBOM に記載する対象の原則は以下のとおり。

| 区分 | 対象 | 除外 |
|---|---|---|
| 自プロジェクトのコンポーネント | **すべて記載**（driver / lib / cli / kernel） | なし |
| 外部ライブラリ | **記載**（バージョン・ライセンスが特定できるもの） | バージョン不明で特定できないもの |
| テスト専用依存 | **記載**（`TEST_DEPENDENCY_OF` で種別を明示） | テストフレームワーク以外のモック等 |
| システムライブラリ | **記載**（ライセンス影響があるもの、§3.2〜3.4 参照） | C 標準ライブラリ（§4.2 参照） |
| ビルドツール | **除外**（gcc, cmake, make 等のコンパイラ・ツール）| — |

---

## 4. Linux システムライブラリの扱い方

### 4.1 記載する 3 カテゴリ

#### (A) glibc — POSIX ランタイムインタフェース

**記載理由**：バイナリに LGPL コードがリンクされ、ライセンス上の影響がある。

本プロジェクトでの使用箇所：

| ヘッダ | 使用ファイル | 機能 |
|---|---|---|
| `<syslog.h>` | `spi-hal/include/logger.hpp` | syslog ロギング |
| `<pthread.h>`（暗黙リンク） | `libsensor/src/sensor.cpp` | スレッド管理（`std::thread` の実装） |
| `<fcntl.h>` | `spi-hal/src/spi_driver.cpp`, `spi-hal/src/kernel_spi_driver.cpp` | `open()`, `O_RDWR` |
| `<unistd.h>` | 同上 | `close()`, `read()`, `write()` |
| `<sys/ioctl.h>` | `spi-hal/src/spi_driver.cpp`, `spi-hal/src/kernel_spi_driver.cpp`, `spi-hal/include/my_spi_dev.h` | ioctl システムコール |

**SBOM での表現**：`DYNAMIC_LINK`（実行時リンク）

#### (B) linux-kernel-headers — Linux カーネルヘッダ

**記載理由**：2 種類の用途が混在し、ライセンス評価が必要。

| ヘッダ種別 | 代表ヘッダ | 適用ライセンス表現 |
|---|---|---|
| UAPI ヘッダ（ユーザー空間用） | `<linux/spi/spidev.h>`, `<linux/ioctl.h>`, `<linux/types.h>` | `GPL-2.0-only WITH Linux-syscall-note` |
| カーネル内部ヘッダ（モジュール内） | `<linux/spi/spi.h>`, `<linux/module.h>`, `<linux/miscdevice.h>` 等 | `GPL-2.0-only`（モジュール自体が GPL） |

> **Linux-syscall-note 例外について**  
> ユーザー空間プログラムが `<linux/*.h>` の UAPI ヘッダをインクルードしても、  
> `Linux-syscall-note` 例外により MIT ライセンスのアプリへ GPL が伝播しない。  
> これは Linux カーネルが意図的に設けた例外であり、ユーザー空間ドライバの MIT ライセンスは有効。

**SBOM での表現**：`BUILD_DEPENDENCY_OF`（コンパイル時のみ、実行時リンクなし）

#### (C) libstdc++ — GNU C++ 標準ライブラリ

**記載理由**：GPL ライセンスだが GCC Runtime Library Exception により伝播しない。アプリのライセンス評価に必要。

**SBOM での表現**：`DYNAMIC_LINK`

### 4.2 記載しなかった Linux 標準ライブラリ

以下は意図的に除外している。

| ライブラリ / ヘッダ | 除外理由 |
|---|---|
| `<cstdlib>`, `<cstdint>`, `<cstring>` 等 C++ ラッパーヘッダ | libstdc++ の一部として記載済み |
| `<iostream>`, `<vector>`, `<string>` 等 STL | 同上 |
| `<cerrno>`, `<cstdio>`, `<cstddef>` | 同上 |
| libc（`libm`, `libdl` 等） | 直接リンクなし。glibc に内包 |
| Linux カーネル自体 | カーネルモジュールのランタイム依存だが、バージョン・配布物が環境依存のため NOASSERTION に吸収 |

> **判断基準**：「ライセンス評価上の影響があるか、または配布先でのコンプライアンスに必要か」を問う。  
> C++ 標準ヘッダ（`<vector>` 等）は libstdc++ として一括記載すれば十分。個別列挙は不要。

### 4.3 追加すべきかどうかの判断フロー

```
新しい #include または外部ライブラリが追加された
          │
          ▼
    それは自プロジェクトのヘッダか？
    ├─ Yes → SBOM 変更不要
    └─ No
          │
          ▼
    既存パッケージ（glibc / linux-kernel-headers / libstdc++）に含まれるか？
    ├─ Yes → SBOM 変更不要（description を必要に応じて更新）
    └─ No
          │
          ▼
    ライセンスが MIT / BSD-3-Clause より制限的か、または
    配布先でのコンプライアンス確認が必要か？
    ├─ Yes → sbom-metadata.json に新パッケージ追加
    └─ No  → 任意（追加しても問題ない）
```

---

## 5. SBOM 更新手順

### 5.1 更新が必要なタイミング

| イベント | 対応 |
|---|---|
| 新しい外部ライブラリを CMakeLists.txt に `find_package()` で追加 | **必須**：新パッケージ追加 |
| 新しいコンポーネント（新しいディレクトリ・バイナリ）を追加 | **必須**：新パッケージ追加 |
| 既存ライブラリのバージョンアップ | **必須**：バージョン・ライセンスを更新 |
| `#include` が増えたが glibc / linux-kernel-headers の範囲内 | 任意（description 更新を推奨） |
| バージョン番号の更新（1.1.0 → 1.2.0 等） | **必須**：`project.version` と各内部パッケージの `version` を更新 |
| ビルドツール変更（gcc バージョン等） | 不要 |

### 5.2 更新コマンド

```bash
# 1. tools/sbom-metadata.json を編集
#    （パッケージ追加・バージョン変更・リレーション追加）

# 2. SBOM ファイルを再生成
./tools/generate-sbom.sh

# 3. 差分確認
git diff sbom.spdx sbom.cdx.json

# 4. 検証
./tools/generate-sbom.sh --verify

# 5. コミット
git add tools/sbom-metadata.json sbom.spdx sbom.cdx.json
git commit -m "docs(sbom): update SBOM for <変更内容>"
```

### 5.3 CI による自動チェック

`.github/workflows/sbom.yml` が以下を自動実行する。

| トリガー | 動作 |
|---|---|
| PR（`CMakeLists.txt` または `sbom-metadata.json` 変更） | `--verify` モードで整合性チェック。不一致なら CI が失敗しコメントが付く |
| main へのプッシュ（同ファイル変更） | SBOM を再生成し自動コミット |
| リリースタグ（`spi-hal/v*`, `lib/v*`, `cli/v*`） | SBOM を再生成し GitHub Release にアセットとして添付 |

---

## 6. カーネルモジュール（my-spi-driver）固有の注意事項

カーネルモジュールは通常のユーザー空間コンポーネントと異なる点がある。

| 項目 | 内容 |
|---|---|
| ライセンス | **GPL-2.0-only**（Linux カーネルはそれ以外のモジュールのロードを公式サポートしない） |
| バージョン | カーネルモジュールのバイナリはビルドしたカーネルバージョンに固有。SBOM には本リポジトリの `version: 1.1.0` を記載し、カーネルバージョンは NOASSERTION |
| CycloneDX type | `firmware`（実行可能バイナリだがユーザー空間アプリではないため） |
| 依存関係 | ユーザー空間ドライバ（`spi-driver`）から `OPTIONAL_DEPENDENCY_OF` として参照。`KernelSpiDriver` 使用時のみカーネルモジュールが必要なため |
| リンク | カーネルモジュールは glibc・libstdc++ にはリンクしない。`linux-kernel-headers` のみが `BUILD_DEPENDENCY_OF` |

---

## 7. 今後の判断基準サマリ

### 追加すべきパッケージ

- 新しいサードパーティライブラリ（バージョン固定、ライセンスが明確）
- プロジェクトの成果物として配布される新コンポーネント

### 追加不要なもの

- gcc / cmake / make 等のビルドツール（SBOM はソフトウェア依存であり、ツールチェーンは対象外）
- `<vector>`, `<string>` 等 C++ 標準ヘッダ（libstdc++ に吸収済み）
- glibc / linux-kernel-headers でカバーされる追加ヘッダ（`<errno.h>` 等）

### syft 導入を再検討するタイミング

- conan (`conanfile.txt`) または vcpkg (`vcpkg.json`) を採用し、  
  管理対象の外部ライブラリが 5 個以上になった場合
- Docker イメージとして配布するようになり、イメージ内のパッケージ列挙が必要になった場合

### ライセンス注意が必要なケース

| ライセンス | 扱い |
|---|---|
| GPL-2.0, GPL-3.0 | ユーザー空間コードへの伝播に注意。例外条項（Linux-syscall-note, GCC-exception-3.1）の有無を確認 |
| LGPL | 動的リンクは一般に許容されるが、静的リンクは要確認 |
| MIT, BSD-2/3-Clause, Apache-2.0 | 商用利用・再配布ともに制限なし |
| カーネルモジュール追加 | GPL-2.0-only で記載。ユーザー空間 MIT との共存は問題ない |
