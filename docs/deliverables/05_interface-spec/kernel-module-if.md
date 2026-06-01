# my_spi_driver カーネルモジュール インターフェース仕様書

| 項目 | 内容 |
|---|---|
| ドキュメント番号 | IF-KRN-001 |
| バージョン | 1.0 |
| 対象 | `my_spi_driver.ko` / デバイスノード `/dev/my_spi_dev` |
| 関連ドキュメント | [DES-DRV-002（カーネルドライバ設計）](../03_detailed-design/kernel-driver-design.md) |

> **位置づけ** — 本書は `my_spi_driver` カーネルモジュールと**ユーザー空間との ABI 契約**（デバイスノード・ioctl・共有構造体・エラーコード）を規定する。モジュール内部の実装・ビルド・Device Tree 適用手順は [DES-DRV-002](../03_detailed-design/kernel-driver-design.md) を参照。
> 共有定義は [`kernel/include/my_spi_dev.h`](../../../kernel/include/my_spi_dev.h)（カーネル・ユーザー空間の両方からインクルード可能）。

---

## 1. デバイスノード

| 項目 | 値 |
|---|---|
| パス | `/dev/my_spi_dev` |
| 種別 | misc キャラクタデバイス（`MISC_DYNAMIC_MINOR`）|
| パーミッション | `0666` |
| 対応操作 | `open(2)` / `close(2)` / `ioctl(2)`（`unlocked_ioctl`）|

`probe()` 時に作成され、`remove()`（`rmmod`）で削除される。

## 2. ioctl コマンド

| コマンド | エンコード | 方向 | 引数型 |
|---|---|---|---|
| `MY_SPI_IOC_CONFIG` | `_IOW('M', 1, struct my_spi_config)` | write-only | `my_spi_config` |
| `MY_SPI_IOC_TRANSFER` | `_IOWR('M', 2, struct my_spi_transfer)` | read/write | `my_spi_transfer` |

- マジックナンバー: `'M'`（`MY_SPI_IOC_MAGIC`）
- `MY_SPI_IOC_CONFIG`: `speed_hz` / `bits_per_word` / `mode` を設定し、カーネル側で `spi_setup()` を呼ぶ。
- `MY_SPI_IOC_TRANSFER`: フルデュプレクス転送を `spi_sync()` で実行する。

## 3. 共有データ構造（ABI）

```c
struct my_spi_config {
    uint32_t speed_hz;      // クロック周波数 [Hz]
    uint8_t  bits_per_word; // ワードビット幅（通常 8）
    uint8_t  mode;          // SPI モード 0〜3
};

struct my_spi_transfer {
    uint64_t tx_buf;        // 送信バッファのユーザー空間アドレス
    uint64_t rx_buf;        // 受信バッファのユーザー空間アドレス
    uint32_t len;           // 転送バイト数（1〜4096）
};
```

> `tx_buf` / `rx_buf` は**ユーザー空間ポインタを `uint64_t` にキャスト**して渡す（32/64bit 環境の互換性のため）。
> `len` の上限は `MY_SPI_MAX_TRANSFER_SIZE = 4096` バイト。

## 4. 動作

| コマンド | カーネル側処理 |
|---|---|
| `MY_SPI_IOC_CONFIG` | `spi->max_speed_hz/bits_per_word/mode` を設定 → `spi_setup()` |
| `MY_SPI_IOC_TRANSFER` | `copy_from_user`(tx) → `spi_sync`(spi_message) → `copy_to_user`(rx) |

いずれも `priv->lock`（mutex）で直列化され、複数プロセスからの同時アクセスは保護される。

## 5. エラーコード（ioctl 戻り値 / errno）

| errno | 条件 |
|---|---|
| `0` | 成功 |
| `EFAULT` | `copy_from_user` / `copy_to_user` 失敗（不正なユーザーポインタ）|
| `EINVAL` | `len == 0` または `len > 4096` |
| `ENOMEM` | カーネル側バッファ確保（`kmalloc`）失敗 |
| `ENOTTY` | 未知の ioctl コマンド |
| `ERESTARTSYS` | mutex 取得待ち中にシグナルで割り込み |
| 負値 | `spi_setup()` / `spi_sync()` が返した SPI サブシステムのエラー |

## 6. Device Tree バインディング（要点）

カーネルモジュールが SPI バスにバインドするための `compatible` 文字列:

| 項目 | 値 |
|---|---|
| `compatible` | `"my,spi-dev"` |
| `reg` | チップセレクト番号（例: `<0>` = CS0）|
| `spi-max-frequency` | 最大周波数 [Hz]（例: `<1000000>`）|

オーバーレイの完全な記述・適用手順は [DES-DRV-002 §6](../03_detailed-design/kernel-driver-design.md) を参照。

## 7. ロード / アンロード

```sh
sudo insmod kernel/my_spi_driver.ko   # ロード → /dev/my_spi_dev 生成
ls -l /dev/my_spi_dev
sudo rmmod my_spi_driver              # アンロード
```

## 8. バージョン履歴

| バージョン | 変更内容 |
|---|---|
| 1.0 | 初版（モジュール VERSION 1.0.0 に対応）|
