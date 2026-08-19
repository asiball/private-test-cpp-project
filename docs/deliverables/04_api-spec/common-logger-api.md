# API仕様書 — 共有ロガー（logger.hpp）

| 項目 | 内容 |
|---|---|
| ドキュメント番号 | API-COM-001 |
| バージョン | 1.0 |
| ヘッダ | `#include <logger.hpp>`（`common/include/`）|
| リンク | なし（ヘッダオンリー / マクロ）|
| 依存 | `<syslog.h>` / `<cstdio>` |

> 全コンポーネント共有のログ基盤。`syslog` をベースに、ビルド種別で出力先を切り替えるマクロ群を提供する。
> `spi-hal/include/logger.hpp` にも**同一 API** の logger があり（spi-hal を取り除いても成立させるため）、`common/` 版は i2c-hal / gpio など新規コンポーネントが依存なしで使う。

---

## 1. 概要

- `syslog` に記録し、DEBUG ビルドでは追加で `stderr` にも出力する。
- 関数ではなく**マクロ**（可変長 `printf` 形式）。例外・戻り値は持たない。

## 2. マクロ一覧

| マクロ | 説明 |
|---|---|
| `LOG_OPEN(ident)` | `openlog()`。`main()` の先頭で 1 回呼ぶ。`ident` はログ識別子 |
| `LOG_CLOSE()` | `closelog()`。`main()` の末尾で呼ぶ |
| `LOGI(fmt, ...)` | 情報ログ（INFO）|
| `LOGW(fmt, ...)` | 警告ログ（WARN）|
| `LOGE(fmt, ...)` | エラーログ（ERROR）|
| `LOGD(fmt, ...)` | デバッグログ（DEBUG）|

`fmt, ...` は `printf` と同じ書式。

## 3. ビルド種別による挙動

| マクロ | DEBUG ビルド（`-DDEBUG`）| RELEASE ビルド（既定）|
|---|---|---|
| `LOGI` | stderr + syslog(`LOG_INFO`) | syslog(`LOG_INFO`) |
| `LOGW` | stderr + syslog(`LOG_WARNING`) | syslog(`LOG_WARNING`) |
| `LOGE` | stderr + syslog(`LOG_ERR`) | syslog(`LOG_ERR`) |
| `LOGD` | stderr のみ | **no-op**（出力なし）|

`LOG_OPEN` は `openlog(ident, LOG_PID | LOG_CONS, LOG_DAEMON)` を展開する（PID 付与・コンソールフォールバック・`LOG_DAEMON` ファシリティ）。

## 4. 命名の理由

`syslog.h` は `LOG_INFO` / `LOG_ERR` / `LOG_DEBUG` を整数マクロとして定義済み。同名マクロを定義するとプリプロセッサが再帰展開して壊れるため、衝突しない `LOGI` / `LOGW` / `LOGE` / `LOGD` を使う。

## 4.1 errno_str()

```cpp
inline const char* errno_str(int err);
```

**説明**: `strerror()` のスレッドセーフ版。エラーメッセージをスレッドローカルバッファに格納して
返すため、複数スレッド（例: CLI の監視スレッドと `read_raw_async()` のワーカースレッド）から
同時に呼び出しても内部バッファが競合しない。`strerror_r` は GNU 版（`char*` を返す）と
XSI/POSIX 版（`int` を返す）でシグネチャが異なるため、内部でオーバーロード解決により両対応する。

| パラメータ / 戻り値 | 説明 |
|---|---|
| `err`（引数） | errno 値 |
| 戻り値 | エラーメッセージ文字列（呼び出したスレッドのローカルバッファを指す。次回呼び出しまで有効） |

```cpp
LOGE("SpiDriver::transfer failed: len=%zu errno=%s", len, errno_str(last_errno_));
```

## 5. 使用例

```cpp
#include <logger.hpp>

int main()
{
    LOG_OPEN("device-ctl");          // 先頭で 1 回
    LOGI("opened %s", path);
    LOGW("retry %d/3", n);
    LOGE("transfer failed: %d", errno_val);
    LOGD("tx[0]=0x%02x", tx[0]);     // DEBUG ビルドのみ出力
    LOG_CLOSE();                     // 末尾で
}
```

## 6. 注意事項

- 書式文字列と引数は `printf` と同じ規則（型不一致は未定義動作）。
- `LOG_OPEN` の `ident` は `openlog` が文字列を保持するため、寿命のある文字列を渡すこと。
- RELEASE では `LOGD` は完全に消える（副作用のある式を `LOGD` に書かない）。

## 7. バージョン履歴

| バージョン | 変更内容 |
|---|---|
| 1.0 | 初版。`LOG_OPEN` / `LOG_CLOSE` / `LOGI` / `LOGW` / `LOGE` / `LOGD` / `errno_str()` |
