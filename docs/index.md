# embedded-device-suite ドキュメント

Linux 組み込みデバイス向けのモノレポ（ドライバ・共有ライブラリ・CLI ツール）です。
このサイトは、**実案件レベルの組み込み SW 開発フローをまるごと体験・学習する**ための資料を、
読みやすい形でまとめたものです。

!!! tip "はじめての方へ"
    まずは **[学習ガイド](guides/learning-guide.md)** から読み始めるのがおすすめです。
    「このリポジトリで何が学べるか」「コードを読む順序」がまとまっています。

## このサイトの歩き方

<div class="grid cards" markdown>

-   :material-school:{ .lg .middle } __学習ガイド__

    ---

    リポジトリの読み方、コードを読む推奨順序、C → C++ のステップアップ。

    [:octicons-arrow-right-24: 学習ガイドへ](guides/learning-guide.md)

-   :material-tools:{ .lg .middle } __ツール別ガイド__

    ---

    CMake / Google Test / 静的解析 / サニタイザー / Doxygen / Docker など。

    [:octicons-arrow-right-24: ビルドガイドへ](guides/tooling/build-guide.md)

-   :material-file-document-multiple:{ .lg .middle } __案件成果物（納品物サンプル）__

    ---

    要件定義 → 基本設計 → 詳細設計 → API/IF 仕様 → テスト → 納品。

    [:octicons-arrow-right-24: 成果物の索引へ](deliverables/README.md)

-   :material-code-braces:{ .lg .middle } __API リファレンス__

    ---

    各 `include/` ヘッダから生成した Doxygen のクラス・関数リファレンス。

    [:octicons-arrow-right-24: API リファレンス（Doxygen）](api/index.html)

</div>

## 開発ライフサイクル

各開発フェーズの成果物が `docs/deliverables/` に揃っており、以下の流れで一貫して追えます。
「なぜこの設計にしたか」を仕様書まで遡って確認できるのが、このプロジェクトの狙いです。

| フェーズ | ドキュメント | 成果物 |
|---|---|---|
| 01 要件定義 | [要件定義書](deliverables/01_requirements/requirements-spec.md) | 機能要件・制約 |
| 02 基本設計 | [システム構成](deliverables/02_basic-design/system-architecture.md) | アーキテクチャ図 |
| 03 詳細設計 | [詳細設計（一覧）](deliverables/README.md) | クラス設計 |
| 04 API 仕様 | [API 仕様（一覧）](deliverables/README.md) | 公開 API |
| 05 IF 仕様 | [IF 仕様（一覧）](deliverables/README.md) | HW 接続・レジスタ仕様 |
| 06 テスト | [テスト計画書](deliverables/06_test/test-plan.md) | テスト計画・仕様 |
| 07 納品 | [リリースノート](deliverables/07_delivery/release-notes/v1.1.0.md) | リリース手順・履歴 |

## C 開発者向けの導線

C 中心の組み込み開発者が C++ へ踏み出すための導入資料です。

- [C → C++ ステップアップガイド](guides/c-to-cpp-stepping-stones.md) — 不透明ポインタ → PIMPL、ops 表 → インターフェース、RAII
- [I2C と ADS1115](guides/i2c-and-ads1115.md) — バス抽象の引き直しと I2C プロトコル
- [GPIO 割り込みと epoll](guides/gpio-interrupts-epoll.md) — ポーリング vs イベント駆動

---

!!! note "このサイトについて"
    このサイトは `docs/` 配下の Markdown から [MkDocs Material](https://squidfunk.github.io/mkdocs-material/) で生成し、
    GitHub Pages に公開しています。仕組み・ローカルでのプレビュー方法は
    [ドキュメントサイト構築ガイド](guides/tooling/docs-site-guide.md) を参照してください。
    ソースコードは [GitHub リポジトリ](https://github.com/asiball/private-test-cpp-project) にあります。
