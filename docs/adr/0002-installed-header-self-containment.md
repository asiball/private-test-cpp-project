# ADR 0002: install したヘッダの自己完結性（クロスコンポーネント include の方針）

- ステータス: Superseded（暫定方針。find_package 対応の実装で解消。下記「更新」参照）
- 日付: 2026-06-13
- 関連: [ADR 0001](0001-optional-independent-components.md) / issue #47 / #20

> **更新（2026-06-13, issue #47 / #20）**: 本 ADR の暫定決定（「install したヘッダの
> 単独利用は未サポート」）は **find_package 対応の実装により解消済み**。
> - 全ライブラリコンポーネントに `install(EXPORT)` + `<pkg>Config.cmake` を整備
>   （共通化ヘルパ `cmake/EdsPackage.cmake`）。利用側は
>   `find_package(sensor)` → `target_link_libraries(app eds::sensor)` で
>   include パスと依存（`find_dependency(spihal)` 等）を継承できる。
> - 公開ヘッダ `sensor.hpp` / `adxl345.hpp` のクロスコンポーネント include を
>   相対パスから**単純名**へ統一（`ads1115.hpp` / `mcp9808.hpp` は元々単純名）。
> - 衝突する「スタンドアロンテストの `-I` 手渡し」問題は、テストの CMake ツリー統合
>   （#20: `-DBUILD_TESTING=ON` / `CMakePresets.json`）を正経路とし、残るレガシー
>   standalone テストには CI で `-I spi-hal/include` を追加して両立させた。
>
> 以降の「決定」「影響」節は当時の暫定判断の記録であり、現状の正は上記「更新」。
> 新コンポーネントの公開ヘッダは**単純名 include**を用いること。

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

**本命案（find_package 対応）を採用し、実装済み**（issue #47、#20 とセットで実施）。
具体的には `install(EXPORT)` + `<pkg>Config.cmake` を整備し（共通化ヘルパ `cmake/EdsPackage.cmake`）、
利用者は `find_package(sensor)` + `target_link_libraries(app eds::sensor)` で include パスと
コンポーネント間依存（`find_dependency(spihal)` 等）を継承する。あわせて公開ヘッダの
クロスコンポーネント include を相対パスから**単純名**へ統一し、インストール済みヘッダを自己完結化した。

> **当初の暫定判断（履歴）**: 本 ADR 初版では「install ヘッダの単独利用は未サポート、利用は
> モノレポ前提」を暫定方針としていた。これは単純名 include 化が当時のスタンドアロンテストの
> `-I` 手渡しと衝突するためだったが、テストの CMake ツリー統合（#20: `-DBUILD_TESTING=ON` /
> `CMakePresets.json`）を正経路としたことで衝突が解消し、本命案を採用できた。レガシー standalone
> テストには CI で `-I spi-hal/include` を補って両立させている。

## 影響（Consequences）

- 利用者は `cmake --install` した成果物を **`find_package` で利用できる**。include パスと依存は
  自動継承され、公開ヘッダは単純名で include できる（`-I` の手渡し不要）。
- 配布物（install 成果物）が自己完結し、「顧客・メンバへ共有・引き継ぐテンプレート」としての
  利用ストーリーが成立する。
- **新コンポーネント追加時は、公開ヘッダのクロスコンポーネント include を単純名で書き**、
  `cmake/EdsPackage.cmake` の `eds_install_package()` で find_package 対応する（CLAUDE.md
  新コンポーネントチェックリスト参照）。相対パス（旧規約）は使わない。

## 代替案（検討した案）

1. **find_package 対応（本命・恒久）**: `install(EXPORT)` + Config.cmake。**← 採用（#47 で実装）**。
2. **install 時にヘッダを単一ディレクトリへフラット集約**し相対 include を単純名へ書換: 暫定策だが
   ソース/インストールで include 規約が二重になり保守が増えるため不採用。
3. **現状維持（install ヘッダ未サポートと明記）**: 初版の暫定方針。#20 の完了により不要となり不採用。
