# ADR 0002: install したヘッダの自己完結性（クロスコンポーネント include の方針）

- ステータス: Accepted（暫定方針。将来 find_package 対応で見直す）
- 日付: 2026-06-13
- 関連: [ADR 0001](0001-optional-independent-components.md) / issue #47 / #20

## 背景（Context）

公開ヘッダのクロスコンポーネント include が **ソースツリーのレイアウト前提**になっており、
`cmake --install` で配布した include ツリーは、パッケージ利用者視点では自己完結していない。

実測（再現）:

```sh
cmake --install build --prefix /tmp/fakeroot
echo '#include <libsensor/sensor.hpp>
int main(){}' > consumer.cpp
g++ -std=c++17 -I/tmp/fakeroot/include -c consumer.cpp
# → fatal error: ../../spi-hal/include/ispi_driver.hpp: No such file or directory
```

原因:

- `libsensor/include/sensor.hpp` / `libadxl345/include/adxl345.hpp` は
  `#include "../../spi-hal/include/ispi_driver.hpp"` という**相対パス**で別コンポーネントを指す。
  install 先（`include/libsensor/`）からは解決できない。
- `libsensor/include/ads1115.hpp` は `#include "ii2c_driver.hpp"` と**単純名**で指す。
  install 先では `include/i2chal/` 配下にあり、利用者が `-I .../include/i2chal` を足さない限り解決できない
  （しかも sensor.hpp の相対パスと規約が食い違う）。

この相対 include は [CLAUDE.md](../../CLAUDE.md) に「スタンドアロンテストが `-I` 無しで解決できるため
**意図的**」と明記された規約だが、その便宜のために**配布物（install 成果物）の正しさを犠牲にしている**。
CI が気づけないのは、テストが install 済み**ライブラリ**には依存しつつ include は常にソースツリーへの
`-I` で通しているため（落とし穴 #1 の裏面）。

影響範囲: `libsensor`（sensor.hpp / ads1115.hpp）と `libadxl345`。
`spi-hal` / `i2c-hal` / `gpio` 単体は自己完結しており問題なし。

## 決定（Decision）

当面は **「install したヘッダの単独利用は未サポート。利用はソースツリー（モノレポ）前提」** とし、
本 ADR と README に明記する（暫定方針）。理由:

- 恒久対応の本命は **`install(EXPORT)` + `xxxConfig.cmake` を整備し、利用者が
  `find_package(spihal)` + `target_link_libraries` で include パスを継承する**形だが、
  これは公開ヘッダを単純名 include へ統一する変更を伴い、スタンドアロンテストの include 解決
  （現行は `-I` 手渡し）と衝突する。
- スタンドアロンテスト側の `-I` 手渡しを廃止する **テストの CMake ツリー統合（issue #20）と
  セットで決める**のが効率的であり、コンポーネントの独立リリース（SOVERSION / タグ）が本格運用
  される前に判断する。

したがって本 ADR では現状を**正しく文書化**するに留め、find_package 対応は #20 と合わせて別途行う。

## 影響（Consequences）

- 利用者は「ヘッダを install して `-I include` だけで使う」ことはできない。モノレポをチェックアウトし、
  トップレベル CMake でビルドする（または各コンポーネントの include ディレクトリを個別に `-I` する）。
- この制限は学習用テンプレートとしては許容するが、**実案件のテンプレートとして配布物の利用ストーリーを
  成立させたい場合は find_package 対応（#47 本命案 / #20）を実施する**こと。
- 新コンポーネント追加時も、公開ヘッダのクロスコンポーネント include は当面この規約（相対パス、
  落とし穴 #1）を踏襲する。

## 代替案（検討したが採用しなかった）

1. **find_package 対応（本命・恒久）**: `install(EXPORT)` + Config.cmake。#20 とセットで実施する想定のため今回は見送り。
2. **install 時にヘッダを単一ディレクトリへフラット集約**し相対 include を単純名へ書換: 暫定策だが
   ソース/インストールで include 規約が二重になり保守が増えるため不採用。
