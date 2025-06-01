# リリースマトリックス（動作確認済み組み合わせ表）

> このページはGitLab Wikiのテンプレートとして使用する。  
> 新しいリリースを出荷するたびに一番上に行を追加し、古い情報は削除しない。

---

## 動作確認済み組み合わせ一覧

| # | 出荷日 | driver | lib | cli | GCC | kernel | ターゲットボード | テスト担当 | GitLab CI | 備考 |
|---|---|---|---|---|---|---|---|---|---|---|
| 3 | 2025-06-01 | v1.0.1 | v1.1.0 | v1.1.0 | 7.5 | 5.10.x | Raspi4B | 山田 | [pipeline/#45](http://gitlab.local/team/embedded-device-suite/-/pipelines/45) | 非同期API追加 |
| 2 | 2025-03-20 | v1.0.1 | v1.0.0 | v1.0.0 | 7.5 | 5.10.x | Raspi4B | 山田 | [pipeline/#28](http://gitlab.local/team/embedded-device-suite/-/pipelines/28) | EAGAIN修正のみ |
| 1 | 2025-01-15 | v1.0.0 | v1.0.0 | v1.0.0 | 7.5 | 5.10.x | Raspi4B | 山田 | [pipeline/#1](http://gitlab.local/team/embedded-device-suite/-/pipelines/1) | 初回リリース |

---

## 既知の非互換組み合わせ

| driver | lib | cli | 問題 | 発見日 | 対応済み |
|---|---|---|---|---|---|
| v1.0.0 | v1.1.0 | v1.1.0 | `read_async` のコールバックでメモリ破壊が発生 | 2025-04-10 | driver/v1.0.1 で修正済み |

---

## 各バージョンの変更概要

### driver

| バージョン | 変更内容 | MR | Redmine |
|---|---|---|---|
| v1.0.1 | EAGAIN時に最大3回リトライ | [MR#7](http://gitlab.local/team/embedded-device-suite/-/merge_requests/7) | #42 |
| v1.0.0 | 初版 SPIキャラクタデバイスドライバ | – | #1 |

### lib

| バージョン | 変更内容 | MR | Redmine |
|---|---|---|---|
| v1.1.0 | `Device::read_async()` 非同期API追加 | [MR#15](http://gitlab.local/team/embedded-device-suite/-/merge_requests/15) | #55 |
| v1.0.0 | 初版 共有ライブラリ | – | #2 |

### cli

| バージョン | 変更内容 | MR | Redmine |
|---|---|---|---|
| v1.1.0 | `--async` フラグを追加 | [MR#16](http://gitlab.local/team/embedded-device-suite/-/merge_requests/16) | #57 |
| v1.0.0 | 初版 CLIツール | – | #3 |

---

## 過去組み合わせの再現手順

特定の出荷バージョンを再現したい場合（デバッグ・ロールバック）:

```bash
# 例: 出荷#2（driver/v1.0.1 + lib/v1.0.0 + cli/v1.0.0）を再現
git clone http://gitlab.local/team/embedded-device-suite.git
cd embedded-device-suite

git checkout driver/v1.0.1
cmake -B build/driver -S driver/
cmake --build build/driver

git checkout lib/v1.0.0
cmake -B build/lib -S lib/
cmake --build build/lib

git checkout cli/v1.0.0
cmake -B build -S . --target device-ctl
cmake --build build
```

または、GitLab CI の該当パイプラインから Artifacts を直接ダウンロードする（ビルド再現不要）。
