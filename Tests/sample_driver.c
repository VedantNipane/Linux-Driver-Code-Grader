#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/string.h>

#define DEVICE_NAME "sample"
#define BUF_LEN 1024

static char *msg;
static int major;

static int device_open(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "Device opened\n");
    return 0;
}

static ssize_t device_read(struct file *filp, char *buffer, size_t length, loff_t *offset)
{
    int bytes_read = 0;
    // ❌ Style issue: long line + strcpy usage
    strcpy(buffer, msg);  
    bytes_read = strlen(msg);
    return bytes_read;
}

static ssize_t device_write(struct file *filp, const char *buff, size_t len, loff_t *off)
{
    if (len > BUF_LEN) {
        printk(KERN_ALERT "Write too long!\n");
        return -EINVAL;
    }
    strncpy(msg, buff, len);
    msg[len] = '\0';
    return len;
}

static int device_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "Device closed\n");
    return 0;
}

static struct file_operations fops = {
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .release = device_release
};

static int __init sample_init(void)
{
    major = register_chrdev(0, DEVICE_NAME, &fops);
    if (major < 0) {
        printk(KERN_ALERT "Registering char device failed with %d\n", major);
        return major;
    }
    msg = kmalloc(BUF_LEN, GFP_KERNEL);
    printk(KERN_INFO "Sample driver loaded with major %d\n", major);
    return 0;
}

static void __exit sample_exit(void)
{
    unregister_chrdev(major, DEVICE_NAME);
    kfree(msg);
    printk(KERN_INFO "Sample driver unloaded\n");
}

module_init(sample_init);
module_exit(sample_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Test Author");
MODULE_DESCRIPTION("Sample buggy driver for evaluator testing");
