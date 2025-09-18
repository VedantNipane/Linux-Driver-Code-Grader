#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#define DEVICE_NAME "sample_good"
#define BUF_LEN 1024

static char *msg;
static int major;

static int device_open(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "Good driver: device opened\n");
    return 0;
}

static ssize_t device_read(struct file *filp, char __user *buffer, size_t length, loff_t *offset)
{
    int bytes_read;
    if (*offset >= strlen(msg))
        return 0; // EOF

    bytes_read = simple_read_from_buffer(buffer, length, offset, msg, strlen(msg));
    return bytes_read;
}

static ssize_t device_write(struct file *filp, const char __user *buff, size_t len, loff_t *off)
{
    if (len > BUF_LEN - 1)
        return -EINVAL;

    if (copy_from_user(msg, buff, len))
        return -EFAULT;

    msg[len] = '\0';
    return len;
}

static int device_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "Good driver: device closed\n");
    return 0;
}

static struct file_operations fops = {
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .release = device_release,
};

static int __init sample_good_init(void)
{
    major = register_chrdev(0, DEVICE_NAME, &fops);
    if (major < 0) {
        printk(KERN_ALERT "Registering good char device failed: %d\n", major);
        return major;
    }
    msg = kmalloc(BUF_LEN, GFP_KERNEL);
    if (!msg)
        return -ENOMEM;
    printk(KERN_INFO "Good sample driver loaded with major %d\n", major);
    return 0;
}

static void __exit sample_good_exit(void)
{
    unregister_chrdev(major, DEVICE_NAME);
    kfree(msg);
    printk(KERN_INFO "Good sample driver unloaded\n");
}

module_init(sample_good_init);
module_exit(sample_good_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Evaluator Test Author");
MODULE_DESCRIPTION("Good sample driver for evaluator testing");
