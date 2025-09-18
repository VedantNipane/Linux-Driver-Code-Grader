/*
 * chardev_ioctl_driver.c
 *
 * Simple Linux character device driver with:
 * - read/write operations
 * - IOCTL support for configuration
 * - proper error handling
 * - buffer management using copy_to_user / copy_from_user
 *
 * Build with the provided Makefile and load with insmod.
 *
 * IOCTL commands supported (user-space macros below):
 *  CHDEV_IOC_CLEAR       - clear the internal buffer
 *  CHDEV_IOC_GET_SIZE    - get current buffer size (returns int)
 *  CHDEV_IOC_SET_SIZE    - set buffer size (takes int, limited by MAX_BUFFER_SIZE)
 *  CHDEV_IOC_GET_DATALEN - get number of valid bytes in buffer (returns int)
 *
 * This is for learning/prototyping only.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/ioctl.h>

#define DEVICE_NAME "chdev_ioctl"
#define CLASS_NAME  "chdev"

#define DEFAULT_BUFFER_SIZE 1024 // 1 KB default
#define MAX_BUFFER_SIZE     (16 * 1024) // 16 KB max allowed

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ChatGPT");
MODULE_DESCRIPTION("Character device with IOCTL and safe copy_to_user/copy_from_user");
MODULE_VERSION("0.2");

/* IOCTL numbers: use magic 'k' */
#define CHDEV_IOC_MAGIC  'k'
#define CHDEV_IOC_CLEAR        _IO(CHDEV_IOC_MAGIC, 0)           /* no args */
#define CHDEV_IOC_GET_SIZE     _IOR(CHDEV_IOC_MAGIC, 1, int)    /* returns int */
#define CHDEV_IOC_SET_SIZE     _IOW(CHDEV_IOC_MAGIC, 2, int)    /* takes int */
#define CHDEV_IOC_GET_DATALEN  _IOR(CHDEV_IOC_MAGIC, 3, int)    /* returns int */

struct chdev_device {
    char *buffer;
    size_t buf_size;   /* current allocated buffer size */
    size_t data_len;   /* bytes of valid data */
    struct cdev cdev;
    struct mutex lock; /* protect buffer and data_len */
};

static dev_t chdev_dev_number;
static struct class *chdev_class;
static struct device *chdev_device;
static struct chdev_device *gdev;

/* forward declarations */
static int chdev_open(struct inode *inode, struct file *file);
static int chdev_release(struct inode *inode, struct file *file);
static ssize_t chdev_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static ssize_t chdev_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos);
static long chdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static loff_t chdev_llseek(struct file *file, loff_t offset, int whence);

static const struct file_operations chdev_fops = {
    .owner = THIS_MODULE,
    .open = chdev_open,
    .release = chdev_release,
    .read = chdev_read,
    .write = chdev_write,
    .unlocked_ioctl = chdev_ioctl,
    .llseek = chdev_llseek,
};

static int chdev_alloc_buffer(struct chdev_device *dev, size_t size)
{
    char *newbuf;

    if (size == 0 || size > MAX_BUFFER_SIZE)
        return -EINVAL;

    newbuf = kzalloc(size, GFP_KERNEL);
    if (!newbuf)
        return -ENOMEM;

    /* free old buffer if exists */
    if (dev->buffer)
        kfree(dev->buffer);

    dev->buffer = newbuf;
    dev->buf_size = size;
    dev->data_len = 0;
    return 0;
}

static int chdev_open(struct inode *inode, struct file *file)
{
    struct chdev_device *dev = container_of(inode->i_cdev, struct chdev_device, cdev);
    file->private_data = dev;
    return 0;
}

static int chdev_release(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t chdev_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    struct chdev_device *dev = file->private_data;
    ssize_t ret = 0;

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    if (*ppos >= dev->data_len) {
        ret = 0; /* EOF */
        goto out;
    }

    if (count > dev->data_len - *ppos)
        count = dev->data_len - *ppos;

    if (copy_to_user(buf, dev->buffer + *ppos, count)) {
        ret = -EFAULT;
        goto out;
    }

    *ppos += count;
    ret = count;

out:
    mutex_unlock(&dev->lock);
    return ret;
}

static ssize_t chdev_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    struct chdev_device *dev = file->private_data;
    ssize_t ret = 0;
    size_t max_writable;

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    if (*ppos >= dev->buf_size) {
        ret = -ENOSPC;
        goto out;
    }

    max_writable = dev->buf_size - *ppos;
    if (count > max_writable)
        count = max_writable;

    if (copy_from_user(dev->buffer + *ppos, buf, count)) {
        ret = -EFAULT;
        goto out;
    }

    *ppos += count;
    if (dev->data_len < *ppos)
        dev->data_len = *ppos;

    ret = count;

out:
    mutex_unlock(&dev->lock);
    return ret;
}

static long chdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct chdev_device *dev = file->private_data;
    int __user *argp = (int __user *)arg;
    int val;
    int ret = 0;

    /* validate magic */
    if (_IOC_TYPE(cmd) != CHDEV_IOC_MAGIC)
        return -ENOTTY;

    switch (cmd) {
    case CHDEV_IOC_CLEAR:
        if (mutex_lock_interruptible(&dev->lock))
            return -ERESTARTSYS;
        memset(dev->buffer, 0, dev->buf_size);
        dev->data_len = 0;
        mutex_unlock(&dev->lock);
        break;

    case CHDEV_IOC_GET_SIZE:
        val = (int)dev->buf_size;
        if (copy_to_user(argp, &val, sizeof(int)))
            ret = -EFAULT;
        break;

    case CHDEV_IOC_GET_DATALEN:
        val = (int)dev->data_len;
        if (copy_to_user(argp, &val, sizeof(int)))
            ret = -EFAULT;
        break;

    case CHDEV_IOC_SET_SIZE:
        /* take an int from user and attempt to resize buffer */
        if (copy_from_user(&val, argp, sizeof(int))) {
            ret = -EFAULT;
            break;
        }

        if (val <= 0 || val > MAX_BUFFER_SIZE) {
            ret = -EINVAL;
            break;
        }

        /* allocate new buffer while holding lock to avoid races */
        if (mutex_lock_interruptible(&dev->lock)) {
            ret = -ERESTARTSYS;
            break;
        }

        /* If new size < current data_len, we drop the overflow data */
        if (val < (int)dev->data_len)
            dev->data_len = val;

        /* try reallocation by allocating new buffer and copying existing data */
        {
            char *newbuf = kzalloc(val, GFP_KERNEL);
            if (!newbuf) {
                ret = -ENOMEM;
                mutex_unlock(&dev->lock);
                break;
            }
            if (dev->data_len)
                memcpy(newbuf, dev->buffer, dev->data_len);
            kfree(dev->buffer);
            dev->buffer = newbuf;
            dev->buf_size = val;
        }
        mutex_unlock(&dev->lock);
        break;

    default:
        ret = -ENOTTY;
        break;
    }

    return ret;
}

static loff_t chdev_llseek(struct file *file, loff_t offset, int whence)
{
    loff_t newpos = 0;

    switch (whence) {
    case SEEK_SET:
        newpos = offset;
        break;
    case SEEK_CUR:
        newpos = file->f_pos + offset;
        break;
    case SEEK_END:
        newpos = gdev->buf_size + offset;
        break;
    default:
        return -EINVAL;
    }

    if (newpos < 0 || newpos > gdev->buf_size)
        return -EINVAL;

    file->f_pos = newpos;
    return newpos;
}

static int __init chdev_init(void)
{
    int ret;
    struct device *dev_ret;

    ret = alloc_chrdev_region(&chdev_dev_number, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("chdev: alloc_chrdev_region failed: %d\n", ret);
        return ret;
    }

    gdev = kzalloc(sizeof(*gdev), GFP_KERNEL);
    if (!gdev) {
        ret = -ENOMEM;
        goto err_unregister;
    }

    mutex_init(&gdev->lock);

    ret = chdev_alloc_buffer(gdev, DEFAULT_BUFFER_SIZE);
    if (ret) {
        pr_err("chdev: buffer allocation failed: %d\n", ret);
        goto err_free_dev;
    }

    cdev_init(&gdev->cdev, &chdev_fops);
    gdev->cdev.owner = THIS_MODULE;

    ret = cdev_add(&gdev->cdev, chdev_dev_number, 1);
    if (ret) {
        pr_err("chdev: cdev_add failed: %d\n", ret);
        goto err_free_buffer;
    }

    chdev_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(chdev_class)) {
        ret = PTR_ERR(chdev_class);
        pr_err("chdev: class_create failed: %d\n", ret);
        goto err_del_cdev;
    }

    dev_ret = device_create(chdev_class, NULL, chdev_dev_number, NULL, DEVICE_NAME);
    if (IS_ERR(dev_ret)) {
        ret = PTR_ERR(dev_ret);
        pr_err("chdev: device_create failed: %d\n", ret);
        goto err_destroy_class;
    }

    pr_info("chdev: loaded (major=%d minor=%d)\n", MAJOR(chdev_dev_number), MINOR(chdev_dev_number));
    return 0;

err_destroy_class:
    class_destroy(chdev_class);
err_del_cdev:
    cdev_del(&gdev->cdev);
err_free_buffer:
    kfree(gdev->buffer);
err_free_dev:
    kfree(gdev);
err_unregister:
    unregister_chrdev_region(chdev_dev_number, 1);
    return ret;
}

static void __exit chdev_exit(void)
{
    device_destroy(chdev_class, chdev_dev_number);
    class_destroy(chdev_class);
    cdev_del(&gdev->cdev);
    kfree(gdev->buffer);
    kfree(gdev);
    unregister_chrdev_region(chdev_dev_number, 1);
    pr_info("chdev: unloaded\n");
}

module_init(chdev_init);
module_exit(chdev_exit);

/*
 * Userspace test helper (compile separately):
 *
 * #include <stdio.h>
 * #include <fcntl.h>
 * #include <sys/ioctl.h>
 * #include "chdev_ioctl_user.h"  // contains the same IOCTL macros
 *
 * int fd = open("/dev/chdev_ioctl", O_RDWR);
 * int size;
 * ioctl(fd, CHDEV_IOC_GET_SIZE, &size);
 * printf("buffer size = %d\n", size);
 */
