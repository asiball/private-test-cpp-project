# リリースマトリックス（動作確認済み組み合わせ表）

> 新しいリリースを出荷するたびに一番上に行を追加し、古い情報は削除しない。

---

## 動作確認済み組み合わせ一覧

| # | 出荷日 | spi-hal | libsensor | cli | GCC | kernel | ターゲットボード | テスト担当 | CI | 備考 |
|---|---|---|---|---|---|---|---|---|---|---|
| 3 | 2025-06-01 | v1.0.1 | v1.1.0 | v1.1.0 | 13 (Ubuntu 24.04) | 5.10.x | RPi 3B+ | 山田 | GitHub Actions | 非同期API追加 |
| 2 | 2025-03-20 | v1.0.1 | v1.0.0 | v1.0.0 | 13 (Ubuntu 24.04) | 5.10.x | RPi 3B+ | 山田 | GitHub Actions | EAGAIN修正のみ |
| 1 | 2025-01-15 | v1.0.0 | v1.0.0 | v1.0.0 | 13 (Ubuntu 24.04) | 5.10.x | RPi 3B+ | 山田 | GitHub Actions | 初回リリース |

---

## 既知の非互換組み合わせ

| spi-hal | libsensor | cli | 問題 | 発見日 | 対応済み |
|---|---|---|---|---|---|
| v1.0.0 | v1.1.0 | v1.1.0 | `read_raw_async` のコールバックでメモリ破壊が発生 | 2025-04-10 | spi-hal/v1.0.1 で修正済み |

---

## 各バージョンの変更概要

### spi-hal

| バージョン | 変更内容 | PR | Issue |
|---|---|---|---|
| v1.0.1 | EAGAIN時に最大3回リトライ | #7 | #42 |
| v1.0.0 | 初版 SPI HAL（ユーザ空間 spidev ラッパ） | – | #1 |

### libsensor

| バージョン | 変更内容 | PR | Issue |
|---|---|---|---|
| v1.1.0 | `Sensor::read_raw_async()` 非同期 API 追加 | #15 | #55 |
| v1.0.0 | 初版 MCP3008 制御ライブラリ | – | #2 |

### cli

| バージョン | 変更内容 | PR | Issue |
|---|---|---|---|
| v1.1.0 | `--async` フラグを追加 | #16 | #57 |
| v1.0.0 | 初版 CLI ツール | – | #3 |

---

## 過去組み合わせの再現手順

特定の出荷バージョンを再現したい場合（デバッグ・ロールバック）:

```bash
# 例: 出荷#2（spi-hal/v1.0.1 + libsensor/v1.0.0 + cli/v1.0.0）を再現
git clone https://github.com/<owner>/<repo>.git
cd <repo>

git checkout spi-hal/v1.0.1
cmake -B build/spihal -S spi-hal/
cmake --build build/spihal

git checkout libsensor/v1.0.0
cmake -B build/libsensor -S libsensor/
cmake --build build/libsensor

git checkout cli/v1.0.0
cmake -B build -S . --target device-ctl
cmake --build build
```

または、GitHub Actions の該当 Workflow run から Artifacts を直接ダウンロードする（ビルド再現不要）。
