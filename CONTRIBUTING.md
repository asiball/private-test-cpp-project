# Contributing Guide

## ブランチ命名規則

| 種別 | パターン | 例 |
|---|---|---|
| 機能追加 | `feature/<topic>` | `feature/async-read` |
| バグ修正 | `fix/<topic>` | `fix/spi-timeout` |
| ドキュメント | `docs/<topic>` | `docs/api-update` |
| リファクタリング | `refactor/<topic>` | `refactor/pimpl-cleanup` |
| CI/ビルド | `ci/<topic>` | `ci/add-sanitizers` |

`main` への直プッシュは禁止。必ず PR を経由すること。

## コミットメッセージ規約

```
<type>: <要約（日本語可、50文字以内）>

<詳細（任意）>
```

**type 一覧:** `feat` / `fix` / `docs` / `refactor` / `test` / `ci` / `chore`

```
# 例
feat: read_async() に完了コールバックを追加
fix: MockSpiDriver の noexcept 指定が抜けていたのを修正
```

## Pull Request の出し方

1. `main` から最新を取得してブランチを切る
2. 変更を実装し、ローカルでテストを通す（`./docker-build.sh`）
3. PR を作成し、以下を記載する:
   - **何を変えたか**（変更の要約）
   - **なぜ変えたか**（背景・動機）
   - **テスト方法**（実施した確認手順）
4. CI（GitHub Actions）がすべて green になることを確認する
5. レビュー後 squash merge またはマージコミットで取り込む

## ローカルビルドと静的解析

```bash
# Docker で全ステップを実行（推奨）
./docker-build.sh

# cppcheck のみ
cppcheck --enable=warning,performance,portability --std=c++17 \
         --suppress=missingIncludeSystem --error-exitcode=1 \
         driver/src/ lib/src/ cli/src/

# clang-tidy（compile_commands.json が必要）
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
clang-tidy -p build/ driver/src/spi_driver.cpp lib/src/device.cpp
```

## コーディング規約

- フォーマットは `.clang-format` に従う（`clang-format -i` で自動整形）
- 静的解析は `.clang-tidy` に定義されたルールに従う
- 公開 API には Doxygen コメントを付与する
- 新規クラスには対応する GTest 単体テストを追加する

## バージョンタグ

コンポーネントごとにタグを付与する:

```bash
git tag driver/vX.Y.Z
git tag lib/vX.Y.Z
git tag cli/vX.Y.Z
git push origin --tags
```

タグがないと `--version` が `v0.0.0-unknown` を表示するため、
リリース時は必ずタグを作成すること。
