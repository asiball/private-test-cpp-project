# コミット規約 & CHANGELOG 自動生成ガイド（Conventional Commits / git-cliff）

| 項目 | 内容 |
|---|---|
| 対象読者 | コミット履歴をリリースノートに直結させたい方 |
| 取り上げる対象 | Conventional Commits 規約、git-cliff（CHANGELOG 生成）、`commit-lint` CI ジョブ |
| 本プロジェクトでの使用箇所 | `CONTRIBUTING.md`、`cliff.toml`、`.github/workflows/ci.yml`（commit-lint）、`.github/workflows/release.yml`（CHANGELOG） |

---

## Conventional Commits とは

コミットメッセージを **`<type>(<scope>): <要約>`** という機械可読な形式に揃える規約。
履歴を自動分類できるため、CHANGELOG・セマンティックバージョニング・リリースノート生成といった下流の自動化が成立する。

```
feat(libsensor): read_raw_async にタイムアウトを追加
fix(spi-hal): open() で fd リークが発生する不具合を修正
docs(api): libsensor-api.md を MCP3008 用に書き直し
```

| type | 用途 |
|---|---|
| `feat`     | 新機能 |
| `fix`      | バグ修正 |
| `docs`     | ドキュメント |
| `refactor` | リファクタリング |
| `test`     | テスト追加・修正 |
| `ci`       | CI/CD 関連 |
| `chore`    | 雑務（依存更新等） |
| `build`    | ビルドシステム（CI ジョブが許容、CONTRIBUTING.md には未掲載） |

`<scope>` は任意。`feat(libsensor): ...` のようにコンポーネント名で絞り込むのが一般的。

---

## 本プロジェクトでの運用

### 規約定義（`CONTRIBUTING.md`）

```
<type>: <要約（日本語可、50文字以内）>

<詳細（任意）>
```

### CI による自動検証（`.github/workflows/ci.yml` の `commit-lint` ジョブ）

PR 作成時に 2 つの検査が走る:

**1. PR タイトルの形式検査**

```bash
PATTERN='^(feat|fix|docs|refactor|test|ci|chore)(\(.+\))?!?: .+'
echo "$TITLE" | grep -qE "$PATTERN" || exit 1
```

**2. PR 内全コミットメッセージの検査**

```bash
PATTERN='^(feat|fix|docs|refactor|test|ci|chore|build)(\(.+\))?!?: .+'
git log origin/main..HEAD --format="%s" | while IFS= read -r msg; do
    echo "$msg" | grep -qE "$PATTERN" || INVALID=1
done
```

> **注意**：PR タイトルは `build` を含まない 7 種、コミットメッセージは `build` を含む 8 種を受理する。
> PR タイトルの validator が `build` を弾く設計のため、`build(scope): ...` を PR タイトルに使うと CI が失敗する。
> 完全に揃えたい場合は `ci.yml` の `Validate PR title format` の正規表現に `build` を追加する。

### Breaking change

`!` を type の直後に付けるとメジャーバージョンの破壊的変更を示す:

```
feat(libsensor)!: read_raw のシグネチャを std::optional に変更
```

---

## git-cliff による CHANGELOG 自動生成

### git-cliff とは

[git-cliff](https://github.com/orhun/git-cliff) は Conventional Commits 形式のコミット履歴を解析し、
タグ間の差分から CHANGELOG.md を生成する Rust 製ツール。**シングルバイナリ**で動くため Rust 環境は不要。

### 設定ファイル（`cliff.toml`）

```toml
[git]
conventional_commits = true
commit_parsers = [
    { message = "^feat",     group = "Features" },
    { message = "^fix",      group = "Bug Fixes" },
    { message = "^docs",     group = "Documentation" },
    { message = "^refactor", group = "Refactoring" },
    { message = "^test",     group = "Tests" },
    { message = "^ci",       group = "CI" },
    { message = "^chore",    group = "Chores" },
    { message = "^build",    group = "Chores" },
    { message = ".*",        skip = true },
]
# コンポーネント別タグをリリース境界として認識
tag_pattern = "^(spi-hal|libsensor|cli)/v[0-9]"
```

**ポイント**:

- `tag_pattern` でモノレポのコンポーネント別タグ（`spi-hal/v*` 等）をリリース境界と認識
- `commit_parsers` の最後に `{ message = ".*", skip = true }` を入れることで、規約外コミットを CHANGELOG から除外
- `[changelog].body` テンプレートは Tera 構文。`scope` ありなら `**scope**: ` を付加するなど、表示を細かく制御できる

### ローカルでの生成

```bash
# git-cliff インストール（Cargo / brew / バイナリダウンロード）
cargo install git-cliff
# または
brew install git-cliff

# 全履歴の CHANGELOG を生成
git cliff --output CHANGELOG.md

# 最新タグ以降の Unreleased セクションのみ
git cliff --unreleased --output CHANGELOG.md

# 特定タグ間の差分
git cliff libsensor/v1.0.0..libsensor/v1.1.0
```

### CI による自動生成（`.github/workflows/release.yml`）

`spi-hal/v*` / `libsensor/v*` / `cli/v*` タグの push をトリガーに、以下を自動実行:

```yaml
- name: Generate CHANGELOG with git-cliff
  uses: orhun/git-cliff-action@v3
  with:
    config: cliff.toml
  env:
    OUTPUT: CHANGELOG.md

- name: Commit CHANGELOG.md          # 生成物を main にコミットバック
  run: |
    git add CHANGELOG.md
    git diff --cached --quiet || git commit -m "chore: update CHANGELOG.md ..."
    git push origin HEAD:main

- name: Create GitHub Release         # CHANGELOG.md をリリースノートに
  uses: softprops/action-gh-release@v2
  with:
    tag_name:  ${{ github.ref_name }}
    body_path: CHANGELOG.md
```

タグ → CHANGELOG → GitHub Release が一連で完結する。

---

## リリースフローの全体像

```
1. 開発者: feat(libsensor): ... のコミットを積む
            ↓
2. PR: commit-lint ジョブが形式を検査
            ↓ merge
3. main: コミットが履歴に積まれる
            ↓
4. リリース担当: git tag libsensor/v1.2.0 && git push --tags
            ↓
5. release.yml: git-cliff が CHANGELOG を生成
            ↓
6. GitHub Release: CHANGELOG をリリースノートとして公開
```

---

## 運用 Tips

- **PR を 1 機能 = 1 コミットにする**と CHANGELOG が読みやすい（Squash merge と相性が良い）
- **`docs:` / `test:` / `chore:` の使い分け**: 仕様書更新は `docs:`、テストコード追加・修正は `test:`、依存更新やフォーマット修正は `chore:`
- **CHANGELOG の手動編集は不可**: タグを打ち直すと `release.yml` が上書きする。文章調整したい場合は `cliff.toml` のテンプレートを編集する
- **規約外コミットを残したい場合**: `cliff.toml` の `{ message = ".*", skip = true }` を `group = "Other"` に変えると CHANGELOG に「その他」セクションが出る
- **マージコミットは除外**: `commit-lint` ジョブ側で `Merge ` で始まるメッセージをスキップしているため、マージコミット形式で merge しても CI は通る
