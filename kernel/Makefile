# カーネルモジュール my_spi_driver のビルド Makefile
#
# 使い方:
#   make                         # 現在の実行中カーネル向けにビルド
#   make KDIR=/path/to/linux     # クロスコンパイル向け（カーネルソース指定）
#   make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-  # Raspberry Pi 向け例
#   make clean

KDIR ?= /lib/modules/$(shell uname -r)/build

obj-m := my_spi_driver.o

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean

install:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules_install
	depmod -a

.PHONY: all clean install
