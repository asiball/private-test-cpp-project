# Docker ビルド環境ガイド

| 項目 | 内容 |
|---|---|
| 対象読者 | ホスト OS を汚さずに本プロジェクトを動かしたい方 |
| 取り上げる対象 | `Dockerfile.build`、`docker-build.sh`、`docker-entrypoint.sh` |
| 本プロジェクトでの使用箇所 | リポジトリルートの 3 ファイル |

---

## なぜ Docker を使うのか

組み込み C++ プロジェクトは、ビルドツールチェーン（特定バージョンの GCC・CMake・Doxygen・pandoc など）を揃えないと再現できないことが多い。Docker を使うと:

- **ホストの環境を汚さない**：apt-get で依存を入れる必要がない
- **再現性が保証される**：CI と同じ Ubuntu 24.04 + 同じパッケージ
- **「私の手元では動くんですけど」を防止**：イメージは誰がビルドしても同じ

```
host                            container
─────                           ─────────
docker-build.sh ──▶ docker build  ──▶ Dockerfile.build
                                       (Ubuntu 24.04 + 全ツール)
                ──▶ docker run -v . ──▶ docker-entrypoint.sh
                                       (6 ステップを実行)
```

---

## 各ファイルの役割

### `Dockerfile.build`

ビルド可能な Ubuntu 24.04 イメージのレシピ。インストールするツール:

| カテゴリ | パッケージ |
|---|---|
| C++ ビルド | `g++`, `cmake`, `make`, `git` |
| 静的解析 | `cppcheck` |
| テスト | `libgtest-dev`（ソースのみ → 後段で自前ビルド） |
| カバレッジ | `gcovr` |
| ドキュメント | `doxygen`, `graphviz` |
| PDF 生成 | `pandoc`, `texlive-xetex`, `texlive-lang-japanese`, `fonts-noto-cjk` |
| SBOM | `python3` |

GTest は Ubuntu の `libgtest-dev` がソースのみ提供する形式のため、続けて `/usr/src/googletest` を cmake でビルドしてシステムに `install` している。

### `docker-build.sh`（ホスト側エントリ）

```bash
#!/usr/bin/env bash
docker build -f Dockerfile.build -t embedded-device-suite-builder .
docker run --rm -v "$(pwd)":/workspace embedded-device-suite-builder
```

- `-v "$(pwd)":/workspace` でリポジトリをコンテナにマウント
- ビルド成果物（`build/`, `output/pdf/`, `test-results/`）はホスト側に直接書き出される

### `docker-entrypoint.sh`（コンテナ内のビルドスクリプト）

`Dockerfile.build` の `CMD` が指す実体。`/workspace` で 6 ステップを順次実行:

| ステップ | 内容 |
|---|---|
| [1/6] | CMake Release ビルド（`build/` 配下） |
| [2/6] | cppcheck 静的解析 |
| [3/6] | spi-hal 単体テスト（JUnit XML 出力） |
| [3b/6] | サニタイザービルド（ASAN + UBSAN）の起動確認 |
| [4/6] | libsensor 単体テスト |
| [5/6] | Doxygen でドキュメント生成 |
| [6/6] | pandoc で設計書 Markdown → PDF 変換 |

各ステップは失敗しても続行する（`|| echo "[警告] ..."`）ため、最後まで通して何が落ちたかを確認しやすい。**CI ではなくローカル開発者向けのスモークテスト**として設計されている。

---

## 使い方

### 推奨フロー（ホストにツールチェーン未導入の場合）

```bash
# リポジトリのトップで 1 行
./docker-build.sh
```

実行後、以下がホスト側に生成される:

| 出力 | パス |
|---|---|
| バイナリ | `build/cli/device-ctl`, `build/libsensor/libsensor.so` |
| テスト結果 | `test-results/*.xml`（JUnit XML） |
| API ドキュメント | `docs/doxygen/html/index.html` |
| PDF 設計書 | `output/pdf/*.pdf` |

### コンテナにシェルで入って試したいとき

```bash
# イメージは事前にビルド済みとする
docker run --rm -it -v "$(pwd)":/workspace \
    embedded-device-suite-builder bash

# コンテナ内で
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=asan
cmake --build build -j$(nproc)
./build/cli/device-ctl --version
```

### イメージを作り直したい（ツール追加後）

```bash
# キャッシュを無視
docker build --no-cache -f Dockerfile.build -t embedded-device-suite-builder .

# または不要なイメージを削除
docker image prune
```

---

## CI との関係

`.github/workflows/ci.yml` は **Docker を使わずに直接 Ubuntu runner で実行**している。理由は以下:

- GitHub Actions の Ubuntu runner はあらかじめ多くのツールが入っており高速
- ジョブごとに並列化したい（lint / docs / coverage / sanitizer を別 runner で）

Docker ビルドは **ローカル開発者向け** であり、CI と二重管理にならないよう、共通の依存リストを `Dockerfile.build` の `apt-get install` と `.github/workflows/ci.yml` の各 `Install dependencies` ステップで保つ必要がある。

---

## 注意点

- `-v "$(pwd)":/workspace` でマウントしたファイルは **コンテナ内 root 権限で書き換えられる**。
  ホスト側で所有者が `root` に変わる場合は `--user $(id -u):$(id -g)` を追加する
- イメージサイズは pandoc + texlive で約 3 GB と大きい。PDF 不要なら `Dockerfile.build` から texlive を削除すると半分以下になる
- Apple Silicon (arm64) Mac でビルドすると、Ubuntu イメージは `linux/arm64` がデフォルト。CI と挙動を揃えたい場合は `--platform linux/amd64` を付ける

---

## 運用 Tips

- イメージのレイヤキャッシュを効かせるため、`apt-get install` の行は変更頻度が低いものをまとめる
- `docker-entrypoint.sh` は失敗時も `set -euo pipefail` で続行できるよう `|| echo "[警告] ..."` を多用している。本番 CI では `set -e` で即時 fail させるべき
- 既存のビルドキャッシュ（`build/`）はコンテナ間で共有される。完全クリーンビルドしたい場合は `rm -rf build` してから `./docker-build.sh`
