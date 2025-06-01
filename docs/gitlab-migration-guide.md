# GitLab 導入・運用ガイド

> **対象読者**: リード開発者（GitLab管理者）、および将来的にGitLabへ移行するメンバー  
> **前提環境**: セルフホストGitLab on Docker（私物デバイス不可のためローカルPC/サブPC上のコンテナ）  
> **現行環境との互換性**: SVN+Redmineは並行稼働させる。本ガイドの範囲内では既存の作業フローへの影響を最小化する。

---

## 目次

1. [GitLab環境のセットアップ](#1-gitlab環境のセットアップ)
2. [日々のGitワークフロー](#2-日々のgitワークフロー)
3. [GitLab CI/CD 設定解説](#3-gitlab-cicd-設定解説)
4. [リリースマトリックスと互換性管理](#4-リリースマトリックスと互換性管理)
5. [後任に感謝されるログ集約運用](#5-後任に感謝されるログ集約運用)

---

## 1. GitLab環境のセットアップ

### 1-1. Docker ComposeでGitLabをローカル起動

```yaml
# docker-compose.yml（サブPC上に配置する）
version: '3'
services:
  gitlab:
    image: gitlab/gitlab-ce:16.11.0-ce.0   # バージョンを固定する
    restart: always
    hostname: 'gitlab.local'
    environment:
      GITLAB_OMNIBUS_CONFIG: |
        external_url 'http://gitlab.local'
        gitlab_rails['time_zone'] = 'Asia/Tokyo'
        # メール通知を無効化（社内メール不使用の場合）
        gitlab_rails['smtp_enable'] = false
    ports:
      - "80:80"
      - "2222:22"   # SSHはホスト2222番に転送（80番競合回避）
    volumes:
      - gitlab_config:/etc/gitlab
      - gitlab_logs:/var/log/gitlab
      - gitlab_data:/var/opt/gitlab

  gitlab-runner:
    image: gitlab/gitlab-runner:alpine
    restart: always
    volumes:
      - /var/run/docker.sock:/var/run/docker.sock
      - gitlab_runner_config:/etc/gitlab-runner

volumes:
  gitlab_config:
  gitlab_logs:
  gitlab_data:
  gitlab_runner_config:
```

```bash
# 起動（初回は数分かかる）
docker compose up -d

# ログ確認
docker compose logs -f gitlab

# 初回rootパスワードの確認
docker exec gitlab grep 'Password:' /etc/gitlab/initial_root_password
```

### 1-2. hostsファイルの設定（開発メンバー全員のPC）

```bash
# /etc/hosts に追記（サブPCのIPアドレスに合わせる）
192.168.1.100  gitlab.local
```

### 1-3. GitLab Runnerの登録

```bash
# Runner登録トークンは GitLab > Admin > Runners で取得
docker exec -it gitlab-runner gitlab-runner register \
  --non-interactive \
  --url "http://gitlab.local" \
  --registration-token "YOUR_REGISTRATION_TOKEN" \
  --executor "docker" \
  --docker-image "gcc:7.5" \
  --description "local-docker-runner" \
  --tag-list "docker" \
  --docker-volumes "/var/run/docker.sock:/var/run/docker.sock"
```

---

## 2. 日々のGitワークフロー

> **方針**: SVNはリード開発者がGitLab経由でのみ更新する。他メンバーはSVNを引き続き使用。

### 2-1. ワークフロー全体図

```
[SVN Server]
     │  svn checkout（他メンバーはここで完結）
     │
[リード開発者のPC]
     │
     ├─ git clone http://gitlab.local/team/embedded-device-suite.git
     │
     │  日次: SVN最新を git pull origin main でローカルに反映
     │        （SVN→Git同期は別途スクリプトで管理）
     │
     ├─ feature/xxx ブランチで開発
     │
     ├─ git push origin feature/xxx
     │        │
     │        └─ GitLab MR作成 → CIパイプライン起動
     │                 │
     │           ビルドOK → セルフレビュー → main へマージ
     │
     └─ main マージ後にSVNへコミット（svn commit）
```

### 2-2. 機能開発の基本コマンド

```bash
# 1. mainを最新にする
git checkout main
git pull origin main

# 2. 機能ブランチを切る
#    命名規則: <type>/<Redmineチケット番号>-<概要>
git checkout -b feature/42-spi-retry-on-eagain

# 3. 開発・コミット
git add driver/src/spi_driver.cpp
git commit -m "fix(driver): EAGAIN時に最大3回リトライする (#42)

SPI転送でEAGAINが返る場合、一時的なリソース不足の可能性がある。
最大3回リトライすることで転送失敗を減らす。

Refs: Redmine #42
Tested: spidev_test -D /dev/spidev0.0 -s 1000000 にてエラー0件を確認"

# 4. GitLabへプッシュ
git push -u origin feature/42-spi-retry-on-eagain

# 5. MR作成後、mainへマージされたらタグを打つ
git checkout main
git pull origin main
git tag -a driver/v1.0.1 -m "driver: EAGAIN リトライ修正 (#42)"
git push origin driver/v1.0.1
```

### 2-3. タグ命名規則

| コンポーネント | タグ形式 | 例 |
|---|---|---|
| SPIドライバ | `driver/vMAJOR.MINOR.PATCH` | `driver/v1.0.1` |
| 共有ライブラリ | `lib/vMAJOR.MINOR.PATCH` | `lib/v1.1.0` |
| CLIツール | `cli/vMAJOR.MINOR.PATCH` | `cli/v1.1.0` |

**バージョン更新基準**:
- `PATCH`: バグ修正（APIに変化なし）
- `MINOR`: 後方互換のある機能追加（`read_async` 追加など）
- `MAJOR`: APIの破壊的変更（libのヘッダ変更など）

```bash
# タグ一覧を見やすく表示
git tag -l --sort=version:refname | grep -E "^(driver|lib|cli)/"
# 出力例:
# driver/v1.0.0
# driver/v1.0.1
# lib/v1.0.0
# lib/v1.1.0
# cli/v1.0.0
# cli/v1.1.0

# 特定タグ時点のファイルを確認
git show driver/v1.0.0:driver/src/spi_driver.cpp

# タグ間の差分
git diff driver/v1.0.0..driver/v1.0.1 -- driver/
```

### 2-4. ロールバック手順

```bash
# 過去バージョンのソースでビルドしたい場合
git checkout driver/v1.0.0
cmake -B build/driver-v1.0.0 -S driver/
cmake --build build/driver-v1.0.0
# → build/driver-v1.0.0/libspi_driver.a が得られる

# 現在のブランチに戻る
git checkout main
```

---

## 3. GitLab CI/CD 設定解説

> 設定ファイル本体: [.gitlab-ci.yml](../.gitlab-ci.yml)

### 3-1. 変更ファイル検知による差分ビルド

```yaml
# cli は lib と driver に依存しているため、それらの変更でも再ビルドする
rules:
  - if: '$CI_PIPELINE_SOURCE == "merge_request_event"'
    changes:
      - cli/**/*
      - lib/**/*    # 依存先が変われば cli も再検証が必要
      - driver/**/*
```

**設計方針**:
- MR時: 変更のあったコンポーネント（＋それに依存するもの）のみビルド
- mainへのpush: 同上（マージ確認用）
- タグpush (`driver/v*` など): 必ずビルド（リリース成果物を生成）

### 3-2. Dockerイメージによるコンパイラ固定

```yaml
variables:
  BUILD_IMAGE: "gcc:7.5"   # GCC 7.5 に固定
```

`gcc:7.5` は C++11/14 をサポートし、組み込み向けのターゲット環境（GCC 4.8〜7系）に近い。
本番ターゲットのGCCバージョンに合わせてイメージを変更すること。

```bash
# 手元でCIと同じ環境でビルド確認する場合
docker run --rm -v $(pwd):/src -w /src gcc:7.5 \
  bash -c "apt-get install -y cmake ninja-build && \
           cmake -B build/driver -S driver/ && \
           cmake --build build/driver"
```

### 3-3. Artifactsの保存

| ジョブ | 成果物 | 保存期間 |
|---|---|---|
| `build:driver` | `build/driver/libspi_driver.a` | 1週間 |
| `build:lib` | `build/lib/libdevice.so*` | 1週間 |
| `build:cli` | `build/device-ctl` | 1週間 |
| `package:release` | `*.tar.gz`（タグ時） | 4週間 |

GitLab UI から Jobs > Download artifacts でバイナリをダウンロード可能。

---

## 4. リリースマトリックスと互換性管理

> 詳細テンプレート: [docs/wiki/release-matrix.md](wiki/release-matrix.md)

### 4-1. 動作確認済み組み合わせ表（概要）

| ビルド日 | driver | lib | cli | GCC | kernel | テスト担当 | 備考 |
|---|---|---|---|---|---|---|---|
| 2025-01-15 | v1.0.0 | v1.0.0 | v1.0.0 | 7.5 | 5.10 | 山田 | 初回リリース |
| 2025-03-20 | v1.0.1 | v1.0.0 | v1.0.0 | 7.5 | 5.10 | 山田 | EAGAIN修正のみ |
| 2025-06-01 | v1.0.1 | v1.1.0 | v1.1.0 | 7.5 | 5.10 | 山田 | 非同期API追加 |

### 4-2. 特定バージョン組み合わせの即時特定

```bash
# 「2025-03-20出荷」のバイナリを再現するコマンド
git checkout driver/v1.0.1
cmake -B build/driver -S driver/

git checkout lib/v1.0.0
cmake -B build/lib -S lib/

git checkout cli/v1.0.0
cmake -B build -S . --target device-ctl
```

または GitLab の CI Artifacts から「そのタグのビルドジョブ」の成果物を直接ダウンロードする。

### 4-3. GitLabタグページでのリリースノート記載

```bash
# タグにリリースノートを付ける（-a オプションで注釈付きタグにする）
git tag -a lib/v1.1.0 -m "lib: 非同期APIを追加 (read_async)

変更内容:
- Device::read_async() を追加 (lib/include/device.hpp)
- 内部でstd::threadを使用。C++11のみで動作する
- 依存: driver/v1.0.1 以上が必要

動作確認:
- libdevice-v1.1.0 + driver-v1.0.1 + cli-v1.1.0 の組み合わせで確認済み
- GCC 7.5, Linux kernel 5.10

互換性:
- lib/v1.0.0 との後方互換あり（既存コードの再コンパイルは不要）"

git push origin lib/v1.1.0
```

---

## 5. 後任に感謝されるログ集約運用

### 5-1. RedmineチケットとGitLab MRの紐付けルール

| 操作 | 記載場所 | 記載内容 |
|---|---|---|
| コミットメッセージ | `Refs: Redmine #42` | チケット番号を末尾に記載 |
| MR タイトル | `fix(driver): EAGAIN リトライ (#42)` | チケット番号をカッコ付きで入れる |
| MR 説明欄 | 後述のテンプレートを使用 | – |
| Redmineチケット | ジャーナルにMR URLを貼る | `GitLab MR: http://gitlab.local/.../merge_requests/7` |

**なぜ両方に記載するのか**: SVNを使い続けるメンバーはRedmineしか見ない。GitLabしか見ない開発者はMRしか見ない。両方に記録することで、どちらから入っても辿れる。

### 5-2. マージリクエスト説明欄テンプレート

```markdown
## 概要
<!-- 1〜3行で「何を・なぜ」変えたか -->
SPIドライバのread処理でEAGAINが返る場合にリトライしていなかったため、
高負荷時に転送失敗が発生していた。最大3回リトライすることで安定性を向上させる。

## 変更ファイル
- `driver/src/spi_driver.cpp`: transfer() にリトライループを追加

## テスト結果
- [ ] ローカルビルド成功 (gcc 7.5, cmake 3.10)
- [ ] `spidev_test -D /dev/spidev0.0 -s 1000000 -p 0xAA` 100回連続 エラー0件
- [ ] CIパイプライン グリーン

## 依存関係・影響範囲
- このPR単体で完結。lib/cli への影響なし
- 次リリース候補: driver/v1.0.1

## 関連
- Redmine: #42
- 参考: https://www.kernel.org/doc/html/latest/spi/spidev.html (EAGAINの説明)
```

### 5-3. MRテンプレートをリポジトリに配置

```bash
# GitLabはこのパスのファイルをMR作成時に自動で読み込む
mkdir -p .gitlab
```

> ファイル: [.gitlab/merge_request_template.md](.gitlab/merge_request_template.md)

### 5-4. SVNしか使えないメンバーの段階的移行ロードマップ

| フェーズ | 期間 | 対象者 | やること | 変化 |
|---|---|---|---|---|
| **Phase 0** | 現在 | リード開発者のみ | GitLab+CIを一人で運用し、SVNへの反映も一人でやる | 他メンバーへの影響ゼロ |
| **Phase 1** | 3〜6ヶ月後 | 全員 | GitLabのMRページを「読む」だけ。コードレビューをGitLab UIで行う | Redmine減・GitLab閲覧が増える |
| **Phase 2** | 6〜12ヶ月後 | 希望者から | `git clone` して `git pull` するだけのリポジトリ参照ワークフロー | SVNへのコミットはリード開発者継続 |
| **Phase 3** | 1年後〜 | 全員 | フルGitワークフロー（feature branch + MR）。SVNを廃止 | SVN完全廃止 |

**Phase 1 の具体的サポート**:
```
メンバーに伝えること:
- ブラウザで http://gitlab.local を開く
- Repositories > embedded-device-suite を選ぶ
- 「Merge Requests」タブを開くとレビュー待ちの変更が見える
- コメントを書けばそのまま記録に残る（Redmineと同じ感覚）
```

**Phase 2 のコマンドチートシート（メンバー向け配布用）**:
```bash
# 初回のみ
git clone http://gitlab.local/team/embedded-device-suite.git
cd embedded-device-suite

# 毎日の作業開始時
git pull origin main

# 特定バージョンのソースを見たい場合
git checkout driver/v1.0.1
# → ファイルがそのバージョンに切り替わる
git checkout main
# → 最新に戻る
```

---

## 付録：よく使うコマンドリファレンス

```bash
# ── ブランチ操作 ──
git branch -a                          # 全ブランチ一覧
git checkout -b feature/XXX            # ブランチ作成
git push -u origin feature/XXX         # 初回push（追跡設定）
git branch -d feature/XXX              # ローカルブランチ削除

# ── タグ操作 ──
git tag -l --sort=version:refname      # タグ一覧（バージョン順）
git tag -a driver/v1.0.0 -m "msg"     # 注釈付きタグ作成
git push origin driver/v1.0.0          # タグをpush
git push origin --tags                 # 全タグをpush
git tag -d driver/v1.0.0              # ローカルタグ削除
git push origin :refs/tags/driver/v1.0.0  # リモートタグ削除

# ── 差分・ログ ──
git log --oneline --graph --all        # グラフ付きログ
git log --oneline -- driver/           # driverのみのログ
git diff driver/v1.0.0..driver/v1.0.1 # タグ間差分
git show driver/v1.0.0                 # タグの詳細

# ── 緊急時のリセット（ローカルのみ） ──
git stash                              # 作業中の変更を一時退避
git stash pop                          # 退避した変更を復元
git checkout main -- driver/src/spi_driver.cpp  # 特定ファイルだけ戻す
```
