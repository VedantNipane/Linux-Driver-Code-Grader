#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#define DEVICE_NAME "strong"
#define BUF_LEN 256

static char *msg;
static int major;
static DEFINE_MUTEX(my_lock);

static int device_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&my_lock))
        return -EBUSY;
    printk(KERN_INFO "Strong driver: opened\n");
    return 0;
}

static ssize_t device_read(struct file *filp, char __user *buffer, size_t length, loff_t *offset)
{
    int bytes_read;
    if (*offset >= strlen(msg))
        return 0;
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
    mutex_unlock(&my_lock);
    printk(KERN_INFO "Strong driver: closed\n");
    return 0;
}

static struct file_operations fops = {
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .release = device_release
};

static int __init strong_init(void)
{
    major = register_chrdev(0, DEVICE_NAME, &fops);
    if (major < 0)
        return major;
    msg = kmalloc(BUF_LEN, GFP_KERNEL);
    if (!msg)
        return -ENOMEM;
    printk(KERN_INFO "Strong driver loaded with major %d\n", major);
    return 0;
}

static void __exit strong_exit(void)
{
    unregister_chrdev(major, DEVICE_NAME);
    kfree(msg);
    printk(KERN_INFO "Strong driver unloaded\n");
}

module_init(strong_init);
module_exit(strong_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Well-Written Driver Example");
MODULE_AUTHOR("Evaluator");
