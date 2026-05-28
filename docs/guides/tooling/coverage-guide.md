# カバレッジ計測ガイド（gcov / gcovr / lcov）

| 項目 | 内容 |
|---|---|
| 対象読者 | C++ テストのカバレッジを計測したい方 |
| 取り上げるツール | gcov（GCC 組込）/ gcovr / lcov |
| 本プロジェクトでの使用箇所 | `.github/workflows/ci.yml` の `coverage` ジョブ |

---

## カバレッジ計測の仕組み

`--coverage` フラグ付きでビルドすると、GCC は実行可能ファイルに **計測コード** を埋め込む。
テストを実行すると `.gcno`（コンパイル時情報）と `.gcda`（実行時情報）が生成され、
gcov / gcovr / lcov がそれらを解析してレポート化する。

```
g++ --coverage -fprofile-arcs -ftest-coverage  → 計測付きバイナリ
                                                 ↓ 実行
                                              .gcda 生成
                                                 ↓
                                       gcovr → HTML / XML レポート
```

| ツール | 役割 |
|---|---|
| **gcov**  | GCC 付属。生 `.gcov` ファイル出力。低レベル |
| **gcovr** | gcov を呼んでサマリ HTML/XML を生成。Python 製。最近の主流 |
| **lcov**  | Perl 製。`.info` を経由して `genhtml` で HTML を作る |

このプロジェクトでは **gcovr** を使用（インストールが簡単で CI 統合が容易）。

---

## 構文の基本

### ビルド（カバレッジ計装あり）

```bash
cmake -S . -B build-cov \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="--coverage -fprofile-arcs -ftest-coverage"
cmake --build build-cov -j$(nproc)
```

### テスト実行（.gcda 生成）

```bash
./build-cov/test-libsensor/test_sensor
# build-cov/ 配下に *.gcda が生成される
```

### gcovr で HTML レポート生成

```bash
gcovr \
    --root . \
    --exclude 'tests/' \
    --exclude '/usr/' \
    --html-details coverage/index.html \
    --xml coverage/cobertura.xml \
    --print-summary
```

| オプション | 内容 |
|---|---|
| `--root` | プロジェクトルート（カバレッジ集計の基点） |
| `--exclude` | 集計から除外するパス（テスト本体、システムヘッダ） |
| `--html-details` | ファイル別の詳細レポートも出力 |
| `--xml` | Cobertura XML（Jenkins / GitHub のレポートビューワが消費） |

---

## 本プロジェクトでの使われ方

`.github/workflows/ci.yml` の `coverage` ジョブが以下を実行:

1. spi-hal と libsensor を `--coverage` フラグ付きでビルド
2. テスト実行
3. `gcovr --root . --exclude 'tests/' --exclude '/usr/'` で HTML / XML 生成
4. `actions/upload-artifact` で `coverage/` を保存

```yaml
- name: Generate coverage report
  run: |
    mkdir -p coverage
    gcovr --root . --exclude 'tests/' --exclude '/usr/' \
      --html-details coverage/index.html \
      --xml coverage/cobertura.xml \
      --print-summary
```

`test-plan.md` のカバレッジ目標は **主要モジュール 80% 以上**。

---

## ローカルでの確認

```bash
# 1. インストール
sudo apt-get install -y gcovr

# 2. カバレッジ付きビルド + テスト
cmake -S . -B build-cov -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="--coverage"
cmake --build build-cov
./build-cov/test-libsensor/test_sensor

# 3. レポート生成
gcovr --root . --exclude 'tests/' --html-details cov.html --print-summary
xdg-open cov.html
```

---

## 運用 Tips

- カバレッジ計測ビルドは **遅い + バイナリが膨らむ**。Release/Debug の通常ビルドとは
  ディレクトリを分けて (`build-cov/`) 共存させる
- ASAN / TSAN ビルドと `--coverage` を併用するとリンクエラーになりがち。
  どちらか単独で実行する
- 「行カバレッジ」と「分岐カバレッジ」は別物。`gcovr --print-summary` の Line / Branch を区別する
- **未到達コードを発見したら**、テストを追加するか、コードを削除するかを判断する。
  「カバーするためのテスト」を機械的に書くと意味のないテストが増える
- `*.gcda` は `.gitignore` 対象（実行のたびに生成される）
