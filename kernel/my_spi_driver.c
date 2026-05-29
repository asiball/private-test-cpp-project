/**
 * @file my_spi_driver.c
 * @brief Linux SPI カーネルデバイスドライバ
 *
 * Linux SPI サブシステムに登録する専用ドライバ。
 * probe() で /dev/my_spi_dev キャラクタデバイスを作成し、
 * ユーザー空間から ioctl 経由でフルデュプレクス SPI 転送を提供する。
 *
 * ビルド: kernel/ ディレクトリで `make` を実行
 * ロード: sudo insmod my_spi_driver.ko
 * 削除:   sudo rmmod my_spi_driver
 *
 * Device Tree オーバーレイ例 (Raspberry Pi):
 *   compatible = "my,spi-dev";
 *   reg = <0>;
 *   spi-max-frequency = <1000000>;
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/spi/spi.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#include "include/my_spi_dev.h"

#define DRIVER_NAME   "my_spi_driver"
#define DEVICE_NAME   "my_spi_dev"
#define MY_SPI_MAX_TRANSFER_SIZE  4096

/* デバイスごとのプライベートデータ */
struct my_spi_priv {
    struct spi_device  *spi;
    struct miscdevice   misc;
    struct mutex        lock;
    struct my_spi_config cfg;
};

/* ---- ファイル操作 ------------------------------------------------- */

static int my_spi_open(struct inode *inode, struct file *filp)
{
    struct miscdevice *misc = filp->private_data;
    struct my_spi_priv *priv =
        container_of(misc, struct my_spi_priv, misc);

    filp->private_data = priv;
    return 0;
}

static int my_spi_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static long my_spi_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct my_spi_priv *priv = filp->private_data;
    void __user *uarg = (void __user *)arg;
    int ret = 0;

    if (mutex_lock_interruptible(&priv->lock))
        return -ERESTARTSYS;

    switch (cmd) {
    case MY_SPI_IOC_CONFIG: {
        struct my_spi_config cfg;
        if (copy_from_user(&cfg, uarg, sizeof(cfg))) {
            ret = -EFAULT;
            break;
        }

        priv->spi->max_speed_hz = cfg.speed_hz;
        priv->spi->bits_per_word = cfg.bits_per_word;
        priv->spi->mode = (u8)cfg.mode;

        ret = spi_setup(priv->spi);
        if (ret < 0) {
            pr_err(DRIVER_NAME ": spi_setup failed: %d\n", ret);
            break;
        }
        priv->cfg = cfg;
        pr_debug(DRIVER_NAME ": config: speed=%u mode=%u bits=%u\n",
                 cfg.speed_hz, cfg.mode, cfg.bits_per_word);
        break;
    }

    case MY_SPI_IOC_TRANSFER: {
        struct my_spi_transfer xfer;
        u8 *tx_buf = NULL;
        u8 *rx_buf = NULL;
        struct spi_transfer t = {};
        struct spi_message  m;

        if (copy_from_user(&xfer, uarg, sizeof(xfer))) {
            ret = -EFAULT;
            break;
        }
        if (xfer.len == 0 || xfer.len > MY_SPI_MAX_TRANSFER_SIZE) {
            ret = -EINVAL;
            break;
        }

        tx_buf = kmalloc(xfer.len, GFP_KERNEL);
        rx_buf = kmalloc(xfer.len, GFP_KERNEL);
        if (!tx_buf || !rx_buf) {
            ret = -ENOMEM;
            goto transfer_cleanup;
        }

        if (copy_from_user(tx_buf, (void __user *)(uintptr_t)xfer.tx_buf, xfer.len)) {
            ret = -EFAULT;
            goto transfer_cleanup;
        }

        t.tx_buf = tx_buf;
        t.rx_buf = rx_buf;
        t.len    = xfer.len;

        spi_message_init(&m);
        spi_message_add_tail(&t, &m);

        ret = spi_sync(priv->spi, &m);
        if (ret < 0) {
            pr_err(DRIVER_NAME ": spi_sync failed: %d\n", ret);
            goto transfer_cleanup;
        }

        if (copy_to_user((void __user *)(uintptr_t)xfer.rx_buf, rx_buf, xfer.len)) {
            ret = -EFAULT;
        }

transfer_cleanup:
        kfree(tx_buf);
        kfree(rx_buf);
        break;
    }

    default:
        ret = -ENOTTY;
        break;
    }

    mutex_unlock(&priv->lock);
    return ret;
}

static const struct file_operations my_spi_fops = {
    .owner          = THIS_MODULE,
    .open           = my_spi_open,
    .release        = my_spi_release,
    .unlocked_ioctl = my_spi_ioctl,
};

/* ---- SPI ドライバ probe/remove ------------------------------------ */

static int my_spi_probe(struct spi_device *spi)
{
    struct my_spi_priv *priv;
    int ret;

    priv = devm_kzalloc(&spi->dev, sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    priv->spi = spi;
    mutex_init(&priv->lock);

    priv->misc.minor = MISC_DYNAMIC_MINOR;
    priv->misc.name  = DEVICE_NAME;
    priv->misc.fops  = &my_spi_fops;
    priv->misc.mode  = 0666;

    ret = misc_register(&priv->misc);
    if (ret) {
        dev_err(&spi->dev, "misc_register failed: %d\n", ret);
        return ret;
    }

    spi_set_drvdata(spi, priv);
    dev_info(&spi->dev, "probed as /dev/%s\n", DEVICE_NAME);
    return 0;
}

static void my_spi_remove(struct spi_device *spi)
{
    struct my_spi_priv *priv = spi_get_drvdata(spi);
    misc_deregister(&priv->misc);
    dev_info(&spi->dev, "removed\n");
}

/* ---- Device Tree マッチングテーブル ------------------------------- */

static const struct of_device_id my_spi_ids[] = {
    { .compatible = "my,spi-dev" },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, my_spi_ids);

static struct spi_driver my_spi_driver_ops = {
    .driver = {
        .name           = DRIVER_NAME,
        .of_match_table = my_spi_ids,
    },
    .probe  = my_spi_probe,
    .remove = my_spi_remove,
};

module_spi_driver(my_spi_driver_ops);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("embedded-device-suite");
MODULE_DESCRIPTION("Custom SPI device driver exposing /dev/my_spi_dev");
MODULE_VERSION("1.0.0");
