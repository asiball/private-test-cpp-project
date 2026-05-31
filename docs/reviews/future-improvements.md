# 今後の課題 — ハマりどころの改善案

[CLAUDE.md](../../CLAUDE.md) に列挙した「ハマりどころ」のうち、構造的に改善できそうなものを
**今後の課題**として整理する。いずれも現状でも CI は通る（回避できている）ため緊急ではないが、
対処すると「知らないと踏む」類の事故を仕組みで防げる。

優先度の目安: 影響度（事故の起きやすさ）× 着手コスト。

---

## 課題一覧

### #1 テストのビルド方式を CMake ツリーに統合する
**現状**: `tests/unit/*` はトップ CMake からビルドされず、ライブラリを `--install` してから
スタンドアロンで configure し、include は CI の `-DCMAKE_CXX_FLAGS="-I..."` で渡している。
CMakeLists 内の `target_include_directories(... ${CMAKE_SOURCE_DIR}/...)` はスタンドアロン
ビルドでは別パスに解決され実質効かない。→ ヘッダ依存を足すたびに CI を直す必要がある。

**改善案**:
- トップ CMake に `option(BUILD_TESTING "" OFF)` + `enable_testing()` を追加し、
  `BUILD_TESTING=ON` のとき `add_subdirectory(tests/...)` する経路を用意する。
  テストは同一ツリー内の**ターゲット**（`spihal` / `i2chal` / `ads1115` 等）に直接リンクでき、
  include は `target_link_libraries` の利用要件（`PUBLIC`/`INTERFACE` include）から自動伝播する。
  → CI の `-I` 列挙が不要になり、落とし穴 #1・#2 が原理的に消える。
- 「install 済みライブラリを名前解決する」現行方式は**パッケージ利用者視点の検証**として価値があるため、
  両立させる（`find_package` 経路を別オプションで残す）。

**影響度: 高 / コスト: 中**（CMake 構成変更 + CI の各テストステップ簡素化）

---

### #2 CI の重複ステップ（build / coverage / sanitizer）を一元化する
**現状**: 同じテストを通常 Debug / `--coverage` / ASAN+UBSAN / TSAN の各フラグで別々に記述。
include パスやビルド手順が各所にコピーされており、1 系統だけ直して他で落ちる事故が起きた
（`tests/unit/libsensor` の `-I i2c-hal/include` 漏れで coverage / sanitizer が赤になった実績）。

**改善案**:
- **CMakePresets.json** を導入し、`debug` / `coverage` / `asan` / `tsan` を
  base preset + 派生で定義。フラグの定義箇所を 1 ファイルに集約する。
- もしくは GitHub Actions の **composite action**（`.github/actions/build-and-test/`）に
  ビルド + テスト手順を切り出し、フラグだけ入力で差し替える。
- あるいは `strategy.matrix` でビルドフラグを行列化する。
- #1 を実施すれば include 重複は解消するため、#1 とセットで検討すると効果が大きい。

**影響度: 高 / コスト: 中**

---

### #3 GTest のソースビルドをキャッシュ / 共有する
**現状**: `build-and-test` / `coverage` / `sanitizer` の各ジョブが毎回
`/usr/src/googletest` をソースからビルドして install している（ジョブ間で重複）。

**改善案**:
- `actions/cache` で GTest のビルド成果物（`/usr/local/lib/libgtest*` 等）をキャッシュする。
- もしくは GTest をプリインストールしたビルド用コンテナイメージ（`Dockerfile.build` 拡張）を使う。
- CI 時間短縮にも効く。

**影響度: 中（主に CI 時間）/ コスト: 低**

---

### #4 SBOM の網羅性を自動チェックする ✅ 対応済み
**もともとの課題**: `tools/generate-sbom.py --verify` は**メタデータと生成物（spdx/cdx）の整合**だけを見ており、
**ソースツリーとの網羅性**は検証しなかった。そのため「コンポーネントを追加したのに
`sbom-metadata.json` に書き忘れる」事故を検出できなかった（実際に `libadxl345` が SBOM 未登録のままになっていた）。

**対応内容**:
- `--verify` に**完全性チェック**（`verify_completeness`）を追加した。トップ `CMakeLists.txt` の
  `foreach(_component ...)` リストを走査し、各コンポーネント（`CMakeLists.txt` が存在＝実際にビルドされるもの）に
  対応する SBOM package が無ければ**失敗**する。対応関係は package の `bom_ref` / `download_location` の
  `#<component>` フラグメントで判定する。
- SBOM 対象外のコンポーネント（`examples` のような install しない学習用デモ）は
  `sbom-metadata.json` の `coverage_policy.exempt_components` に明示列挙する。
  → 新規コンポーネントは「package を足す」か「除外に追記する」かのどちらかを必須化できる。
- CI は既存の `Verify SBOM consistency` ジョブで `--verify` を実行しているため、追加の CI 変更は不要。

**残課題（任意）**: メタデータの package 一覧を CMake ターゲットから半自動生成する（コスト中・優先度低）。

---

### #5 既存レビューからの継続課題（再掲・リンク）
コード側の改善候補は [code-ci-review-2026-05.md](code-ci-review-2026-05.md) の
「未対応（将来の改善候補）」にまとまっている（`read_raw_async` の所有権 / use-after-free、
`transfer()` 検証ロジックの共通化、`strerror_r()` 化、EAGAIN バックオフ等）。本ファイルは
**ビルド / CI / ツール基盤**側の課題を扱う。

---

## まとめ（着手順の提案）

1. **#1 + #2**（テストの CMake 統合 + CI 一元化）— 落とし穴の根を断つ。最優先。**未着手**。
2. ~~**#4**（SBOM 網羅性チェック）~~ — ✅ 対応済み（`--verify` の完全性チェック）。
3. **#3**（GTest キャッシュ）— CI 体験の改善。**未着手**。
