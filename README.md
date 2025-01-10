# embedded-device-suite

Linux組み込みデバイス向けモノレポ。ドライバ・共有ライブラリ・CLIツールを一元管理する。

## コンポーネント

| コンポーネント | ディレクトリ | 説明 |
|---|---|---|
| spi-driver | `driver/` | Linux SPIキャラクタデバイスドライバ (C++11) |
| libdevice | `lib/` | ユーザ空間向け共有ライブラリ |
| device-ctl | `cli/` | デバイス操作CLIツール |

## ビルド要件

- GCC 4.8以上（C++11対応）
- CMake 3.10以上
- Linux kernel headers（driverのみ）

## クイックスタート

```bash
# 全コンポーネントビルド
cmake -B build -DCMAKE_BUILD_TYPE=Release .
cmake --build build

# 個別ビルド
cmake -B build/driver -S driver/
cmake -B build/lib    -S lib/
cmake -B build/cli    -S cli/
```

## タグ命名規則

```
<component>/v<MAJOR>.<MINOR>.<PATCH>

例:
  driver/v1.0.0
  lib/v1.0.0
  cli/v1.0.0
```

## ドキュメント

- [GitLab移行・運用ガイド](docs/gitlab-migration-guide.md)
- [リリースマトリックス](docs/wiki/release-matrix.md)
