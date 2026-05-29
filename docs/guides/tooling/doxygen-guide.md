# Doxygen ガイド

| 項目 | 内容 |
|---|---|
| 対象読者 | C/C++ ヘッダから API ドキュメントを自動生成したい方 |
| バージョン | Doxygen 1.9 以上 |
| 本プロジェクトでの使用箇所 | ルート `Doxyfile`、各ヘッダのコメント、`docs/doxygen/`（生成物） |

---

## Doxygen とは

ソースコード内のコメント（`/** ... */`）を解析して HTML や PDF の API ドキュメントを
生成するツール。**仕様書とコードを別ファイルで管理しない**ことで、ドキュメント腐敗を防ぐ。

```
foo.hpp の /** @brief ... */ コメント
      ↓ doxygen Doxyfile
docs/doxygen/html/index.html
```

---

## 必須の構文

### コメントの書き方（よく使うタグ）

```cpp
/**
 * @brief 関数の一行サマリ
 *
 * 詳細説明。複数行可。
 *
 * @param  name  引数の説明
 * @return 戻り値の説明
 * @note   注意点
 * @warning 警告
 * @see    関連する関数や別ファイル
 * @snippet test_sample.cpp ID-001   別ファイルからコードを取り込む
 */
[[nodiscard]] int do_something(const std::string& name);
```

### Doxyfile の主要キー

```ini
INPUT                  = include src    # 解析対象ディレクトリ（空白区切り）
RECURSIVE              = YES
FILE_PATTERNS          = *.hpp *.cpp
EXAMPLE_PATH           = tests          # @snippet が探すパス
USE_MDFILE_AS_MAINPAGE = README.md      # トップページに使う Markdown
HTML_EXTRA_STYLESHEET  = my-theme.css   # 独自テーマ
GENERATE_HTML          = YES
HAVE_DOT               = YES            # Graphviz でクラス図/コールグラフ
WARN_IF_UNDOCUMENTED   = YES            # 未ドキュメントを警告（CI でフェイル可）
```

---

## 本プロジェクトでの使われ方

### @snippet でテストコードを API ドキュメントに引用

ヘッダ側:

```cpp
/**
 * @brief デバイスをオープンする
 *
 * **テストケース（UT-LIB-002）** — 無効なパスでは false を返す:
 * @snippet test_sensor.cpp UT-LIB-002
 */
[[nodiscard]] bool open() noexcept;
```

テスト側（`tests/unit/libsensor/test_sensor.cpp`）:

```cpp
//! [UT-LIB-002]
TEST(SensorOpen, InvalidDeviceReturnsFalse) {
    Sensor s("/dev/spidevXX.0");
    EXPECT_FALSE(s.open());
}
//! [UT-LIB-002]
```

`Doxyfile` の `EXAMPLE_PATH` にテストディレクトリを指定すると、
生成された HTML に **動くテストコード** がそのまま埋め込まれる。
ドキュメントのサンプルコードが古くなる問題を構造的に防げる。

### 設定（抜粋: `Doxyfile`）

```ini
INPUT                  = spi-hal/include libsensor/include cli/src
EXAMPLE_PATH           = tests/unit/spi-hal tests/unit/libsensor
USE_MDFILE_AS_MAINPAGE = README.md
HTML_EXTRA_STYLESHEET  = docs/assets/doxygen-awesome.css
HAVE_DOT               = YES
UML_LOOK               = YES
```

### CI での自動生成

`.github/workflows/ci.yml` の `docs` ジョブが `doxygen Doxyfile` を実行し、
`docs/doxygen/html/` を artifact として保存する。PR ごとに最新版が閲覧可能。

---

## 実行コマンド

```bash
# Graphviz がない環境ではクラス図がスキップされる
sudo apt-get install -y doxygen graphviz

# 生成
doxygen Doxyfile

# ブラウザで確認
xdg-open docs/doxygen/html/index.html
```

`WARN_IF_UNDOCUMENTED=YES` 設定では未ドキュメントが warning ログ
（`docs/doxygen/doxygen-warnings.log`）に記録される。

---

## 運用 Tips

- `docs/doxygen/` は **`.gitignore` 対象**（生成物）
- 設計書本文（Markdown）を `INPUT += docs/...` に追加すると、Doxygen 上で
  クラス API と設計書が連結された 1 サイトとして公開できる
- C++ では `///` 系の 1 行コメントも可。チームで統一すること
- 「未ドキュメントを CI で失敗」させたい場合は `WARN_AS_ERROR = YES` か、warnings.log の
  非空チェックを追加する
