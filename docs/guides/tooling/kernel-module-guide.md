# Linux カーネルモジュール ガイド

| 項目 | 内容 |
|---|---|
| 対象読者 | C は書けるが、カーネル空間のコードに触れたことがない方 |
| 取り上げる対象 | Linux キャラクタデバイスドライバ + ioctl |
| 本プロジェクトでの使用箇所 | `kernel/my_spi_driver.c`、`kernel/Makefile`、`kernel/include/my_spi_dev.h` |

---

## カーネルモジュールとは

Linux カーネルに**動的にロード/アンロード**できるオブジェクト（`.ko` ファイル）。
ユーザ空間アプリと違って:

- `glibc` は使えない（`printk` / `kmalloc` 等のカーネル API のみ）
- **エラー処理を即座に正確に行う必要がある**（カーネルパニックを引き起こす可能性がある）
- ライセンスは GPL 互換が必須（`MODULE_LICENSE("GPL")`）

```
my_spi_driver.c  →  make → my_spi_driver.ko
                              ↓ insmod
                       /dev/my_spi_dev を生成
                              ↓ open/ioctl/close（ユーザ空間から）
                          spi-hal/src/kernel_spi_driver.cpp
```

---

## 構文の基本

### 最小のキャラクタデバイスドライバ

```c
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>

static int my_open(struct inode *ip, struct file *fp)  { return 0; }
static int my_release(struct inode *ip, struct file *fp) { return 0; }
static ssize_t my_read(struct file *fp, char __user *buf, size_t len, loff_t *off) {
    return 0;   // 0 = EOF
}

static const struct file_operations my_fops = {
    .owner   = THIS_MODULE,
    .open    = my_open,
    .release = my_release,
    .read    = my_read,
    .unlocked_ioctl = my_ioctl,
};

static int __init my_init(void) {
    pr_info("my_driver: loaded\n");
    /* register_chrdev / cdev_init / device_create … */
    return 0;
}

static void __exit my_exit(void) {
    pr_info("my_driver: unloaded\n");
    /* cdev_del / unregister_chrdev_region … */
}

module_init(my_init);
module_exit(my_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("...");
MODULE_DESCRIPTION("...");
```

### ioctl 定義（ユーザ空間と共有）

ioctl 番号は **ユーザ空間とカーネル空間で同じヘッダを共有** する必要がある。

```c
// my_spi_dev.h
#define MY_SPI_IOC_MAGIC  'm'
#define MY_SPI_IOC_SET_CONFIG  _IOW(MY_SPI_IOC_MAGIC, 1, struct my_spi_config)
#define MY_SPI_IOC_TRANSFER    _IOWR(MY_SPI_IOC_MAGIC, 2, struct my_spi_transfer)
```

ユーザ空間（C++）からは:

```cpp
#include "my_spi_dev.h"
struct my_spi_transfer xfer = { .tx_buf = tx, .rx_buf = rx, .len = 3 };
ioctl(fd, MY_SPI_IOC_TRANSFER, &xfer);
```

### ビルド（Out-of-tree モジュール）

```makefile
KDIR ?= /lib/modules/$(shell uname -r)/build
obj-m := my_spi_driver.o

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
```

```bash
cd kernel/
make
sudo insmod my_spi_driver.ko
ls /dev/my_spi_dev          # デバイスファイル生成確認
sudo rmmod my_spi_driver
```

---

## 本プロジェクトでの使われ方

このプロジェクトには Linux 標準の `spidev` を使う実装（`SpiDriver`）と、
**専用カーネルドライバ** を介す実装（`KernelSpiDriver`）の 2 系統がある。
`KernelSpiDriver` は `kernel/my_spi_driver.ko` をロードしてから使う。

```
ユーザ空間              カーネル空間
────────────       ───────────────
SpiDriver  ──→ /dev/spidev0.0  ──→ Linux 標準 spidev サブシステム
KernelSpiDriver ─→ /dev/my_spi_dev ─→ my_spi_driver.ko（本プロジェクト固有）
```

### ファイル構成

```
kernel/
├── Makefile                 # KDIR を使った out-of-tree ビルド
├── my_spi_driver.c          # カーネル側ドライバ実装
└── include/
    └── my_spi_dev.h         # ioctl 定義（カーネル/ユーザ空間共有）

spi-hal/include/kernel_spi_driver.hpp  ← ユーザ空間ラッパ
spi-hal/src/kernel_spi_driver.cpp      ← ioctl 呼び出し実装
```

### 共有ヘッダ参照のしくみ

`spi-hal/CMakeLists.txt` で `kernel/include` を include path に追加し、
ユーザ空間側からも `#include "my_spi_dev.h"` できるようにしている:

```cmake
target_include_directories(spihal
    PUBLIC  include
    PRIVATE ../kernel/include   # ioctl 定義の共有
)
```

これにより、ioctl 番号の不整合が発生し得ない（ヘッダが 1 つしかない）。

---

## 注意点

- ユーザ空間とカーネル空間で **同じ構造体レイアウトを保証**する必要がある。
  `pragma pack` や `__attribute__((packed))` を明示する場合がある
- カーネルバージョンによって API が変わる（5.x と 6.x で関数シグネチャが違うことも）
- カーネルパニックすると**マシンが固まる**。VM やリモート実機で開発するのが安全
- デバッグログは `printk` → `dmesg` で確認

---

## 運用 Tips

- `MODULE_LICENSE("GPL")` を忘れると、GPL シンボル（多くのカーネル API）にリンクできない
- `Makefile` で `KDIR` を上書きすることでクロスコンパイル可能:
  ```bash
  make KDIR=/path/to/raspi-linux ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-
  ```
- カーネルモジュールは**ユニットテストできない**。動作確認は `insmod` + 結合テストに頼る
- `kallsyms` / `addr2line` でクラッシュ時のスタックトレースをソース行に対応付ける
