# embedded-device-suite 学習ガイド

Linux 組み込みデバイス向けモノレポ（ドライバ・共有ライブラリ・CLI ツール）の **学習ガイド集**です。
**C 中心の組み込み開発者が C++ へ踏み出す**ことを主眼に、設計パターン・ツールチェーン・運用フローを
読み物としてまとめています。

!!! tip "はじめての方へ"
    まずは **[学習ガイド（全体トップ）](learning-guide.md)** から読み始めるのがおすすめです。
    「このリポジトリで何が学べるか」「コードを読む順序」がまとまっています。

## このサイトの歩き方

<div class="grid cards" markdown>

-   :material-school:{ .lg .middle } __学習ガイド（全体トップ）__

    ---

    リポジトリの読み方、コードを読む推奨順序、開発フェーズとの対応。

    [:octicons-arrow-right-24: 学習ガイドへ](learning-guide.md)

-   :material-language-cpp:{ .lg .middle } __C → C++ ステップアップ__

    ---

    不透明ポインタ → PIMPL、ops 表 → インターフェース、RAII。

    [:octicons-arrow-right-24: ステップアップガイドへ](c-to-cpp-stepping-stones.md)

-   :material-tools:{ .lg .middle } __ツール別ガイド__

    ---

    CMake / Google Test / 静的解析 / サニタイザー / Doxygen / Docker など。

    [:octicons-arrow-right-24: ビルドガイドへ](tooling/build-guide.md)

-   :material-code-braces:{ .lg .middle } __API リファレンス__

    ---

    各 `include/` ヘッダから生成した Doxygen のクラス・関数リファレンス。

    [:octicons-arrow-right-24: API リファレンス（Doxygen）](api/index.html)

</div>

## C 開発者向けの導線

- [C → C++ ステップアップガイド](c-to-cpp-stepping-stones.md) — 不透明ポインタ → PIMPL、ops 表 → インターフェース、RAII
- [I2C と ADS1115](i2c-and-ads1115.md) — バス抽象の引き直しと I2C プロトコル
- [GPIO 割り込みと epoll](gpio-interrupts-epoll.md) — ポーリング vs イベント駆動
- [Rust 移行ガイド](rust-migration-guide.md) ・ [SBOM ガイド](sbom-guide.md)

## プロジェクト全体（要件 → 設計 → テスト → 納品）を見るには

このサイトは**学習ガイドに特化**しています。実案件レベルの開発フローを示す**案件成果物**
（要件定義・基本/詳細設計・API/IF 仕様・テスト計画・リリースノート）は、リポジトリ内の
`docs/deliverables/` にあります（PDF/Word への変換も可能）。

- [📑 案件成果物の索引（GitHub）](../deliverables/README.md) — 読者別の入口・コンポーネント対応表
- [GitHub リポジトリ](https://github.com/asiball/private-test-cpp-project)

> 学習ガイドの本文から成果物やソースコードへ張られたリンクは、自動的に GitHub の該当ファイルへ移動します。

---

!!! note "このサイトについて"
    `docs/guides/` 配下の Markdown から [MkDocs Material](https://squidfunk.github.io/mkdocs-material/) で生成し、
    GitHub Pages に公開しています。仕組み・ローカルでのプレビュー方法は
    [ドキュメントサイト構築ガイド](tooling/docs-site-guide.md) を参照してください。
