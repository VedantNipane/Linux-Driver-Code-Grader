// simple_chardev.c
// A minimal char device with a 1KB internal buffer and basic read/write support.

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#define DEVICE_NAME "simplechardev"
#define CLASS_NAME  "simplechar"
#define BUFFER_SIZE 1024

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ChatGPT");
MODULE_DESCRIPTION("Simple character device with 1KB buffer");
MODULE_VERSION("0.1");

static dev_t dev_num;
static struct class *simplechar_class;
static struct device *simplechar_device;

struct simple_dev {
    char *buffer;             // internal 1KB buffer
    size_t data_len;          // how many valid bytes stored
    struct cdev cdev;
    struct mutex lock;        // protect buffer + data_len
};

static struct simple_dev *sdev;

/* File operations */

static int simple_open(struct inode *inode, struct file *file)
{
    struct simple_dev *dev = container_of(inode->i_cdev, struct simple_dev, cdev);
    file->private_data = dev;
    return 0;
}

static int simple_release(struct inode *inode, struct file *file)
{
    return 0;
}

/*
 * Read: copies up to 'count' bytes from device buffer starting at *ppos into user buffer.
 * Behavior: reading beyond data_len returns 0 (EOF).
 */
static ssize_t simple_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos)
{
    struct simple_dev *dev = file->private_data;
    ssize_t retval = 0;

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    if (*ppos >= dev->data_len) {
        /* nothing left to read */
        retval = 0;
        goto out;
    }

    /* limit read to available data */
    if (count > dev->data_len - *ppos)
        count = dev->data_len - *ppos;

    if (copy_to_user(ubuf, dev->buffer + *ppos, count)) {
        retval = -EFAULT;
        goto out;
    }

    *ppos += count;
    retval = count;

out:
    mutex_unlock(&dev->lock);
    return retval;
}

/*
 * Write: copies up to 'count' bytes from user buffer into device buffer starting at *ppos.
 * Behavior: will not exceed BUFFER_SIZE; if writing past end, only the portion fitting into buffer is written.
 * After write, data_len updated to max(data_len, ppos + written).
 */
static ssize_t simple_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos)
{
    struct simple_dev *dev = file->private_data;
    ssize_t retval = 0;
    size_t max_can_write;

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    if (*ppos >= BUFFER_SIZE) {
        retval = -ENOSPC;
        goto out;
    }

    max_can_write = BUFFER_SIZE - *ppos;
    if (count > max_can_write)
        count = max_can_write;

    if (copy_from_user(dev->buffer + *ppos, ubuf, count)) {
        retval = -EFAULT;
        goto out;
    }

    *ppos += count;
    if (dev->data_len < *ppos)
        dev->data_len = *ppos;

    retval = count;

out:
    mutex_unlock(&dev->lock);
    return retval;
}

/* Optional: support llseek so userspace can set the position */
static loff_t simple_llseek(struct file *file, loff_t offset, int whence)
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
        newpos = BUFFER_SIZE + offset;
        break;
    default:
        return -EINVAL;
    }

    if (newpos < 0 || newpos > BUFFER_SIZE)
        return -EINVAL;

    file->f_pos = newpos;
    return newpos;
}

static const struct file_operations simple_fops = {
    .owner   = THIS_MODULE,
    .open    = simple_open,
    .release = simple_release,
    .read    = simple_read,
    .write   = simple_write,
    .llseek  = simple_llseek,
};

static int __init simple_init(void)
{
    int ret;
    struct device *dev_ret;

    /* allocate device number */
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("simplechardev: failed to allocate char dev region\n");
        return ret;
    }

    /* allocate device struct */
    sdev = kzalloc(sizeof(*sdev), GFP_KERNEL);
    if (!sdev) {
        ret = -ENOMEM;
        goto unregister_region;
    }

    sdev->buffer = kzalloc(BUFFER_SIZE, GFP_KERNEL);
    if (!sdev->buffer) {
        ret = -ENOMEM;
        goto free_sdev;
    }
    sdev->data_len = 0;
    mutex_init(&sdev->lock);

    cdev_init(&sdev->cdev, &simple_fops);
    sdev->cdev.owner = THIS_MODULE;

    ret = cdev_add(&sdev->cdev, dev_num, 1);
    if (ret) {
        pr_err("simplechardev: cdev_add failed\n");
        goto free_buffer;
    }

    simplechar_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(simplechar_class)) {
        ret = PTR_ERR(simplechar_class);
        pr_err("simplechardev: class_create failed\n");
        goto del_cdev;
    }

    dev_ret = device_create(simplechar_class, NULL, dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(dev_ret)) {
        ret = PTR_ERR(dev_ret);
        pr_err("simplechardev: device_create failed\n");
        goto destroy_class;
    }

    pr_info("simplechardev: loaded (dev=%d:%d)\n", MAJOR(dev_num), MINOR(dev_num));
    return 0;

destroy_class:
    class_destroy(simplechar_class);
del_cdev:
    cdev_del(&sdev->cdev);
free_buffer:
    kfree(sdev->buffer);
free_sdev:
    kfree(sdev);
unregister_region:
    unregister_chrdev_region(dev_num, 1);
    return ret;
}

static void __exit simple_exit(void)
{
    device_destroy(simplechar_class, dev_num);
    class_destroy(simplechar_class);
    cdev_del(&sdev->cdev);
    kfree(sdev->buffer);
    kfree(sdev);
    unregister_chrdev_region(dev_num, 1);
    pr_info("simplechardev: unloaded\n");
}

module_init(simple_init);
module_exit(simple_exit);
