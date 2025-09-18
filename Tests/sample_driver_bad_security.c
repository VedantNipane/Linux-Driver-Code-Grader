#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#define DEVICE_NAME "badsec"
#define BUF_LEN 128

static char *msg;
static int major;
static struct mutex my_lock;

static int device_open(struct inode *inode, struct file *file)
{
    mutex_lock(&my_lock); // ❌ never unlocked
    printk(KERN_INFO "BadSec driver: opened\n");
    return 0;
}

static ssize_t device_read(struct file *filp, char *buffer, size_t length, loff_t *offset)
{
    // ❌ strcpy (unsafe)
    strcpy(buffer, msg);
    return strlen(msg);
}

static ssize_t device_write(struct file *filp, const char *buff, size_t len, loff_t *off)
{
    // ❌ unchecked copy_from_user
    copy_from_user(msg, buff, len);
    return len;
}

static int device_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "BadSec driver: closed\n");
    return 0;
}

static struct file_operations fops = {
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .release = device_release
};

static int __init badsec_init(void)
{
    major = register_chrdev(0, DEVICE_NAME, &fops);
    msg = kmalloc(BUF_LEN, GFP_KERNEL); // ❌ never freed
    printk(KERN_INFO "BadSec driver loaded\n");
    return 0;
}

static void __exit badsec_exit(void)
{
    unregister_chrdev(major, DEVICE_NAME);
    printk(KERN_INFO "BadSec driver unloaded\n");
}

module_init(badsec_init);
module_exit(badsec_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Bad Security Driver Example");
MODULE_AUTHOR("Evaluator");
