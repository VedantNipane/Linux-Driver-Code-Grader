#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/slab.h>

// Module Information
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple character device driver.");
MODULE_VERSION("1.0");

// IOCTL command definitions
// The_IO macro defines an IOCTL command number
#define WR_VALUE _IOW('a', 'a', int32_t*)
#define RD_VALUE _IOR('a', 'b', int32_t*)

// Driver-specific macros
#define DEVICE_NAME "simple_char_dev"
#define CLASS_NAME "simple_char_class"
#define BUFFER_SIZE 1024

// Global variables for the driver
static int major_number;
static struct class* simple_char_class = NULL;
static struct device* simple_char_device = NULL;
static struct cdev simple_char_cdev;
static char *device_buffer;
static int buffer_len = 0;
static int32_t ioctl_value = 0;

// Device file operations
static int simple_char_open(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "simple_char_dev: Device opened.\n");
    return 0;
}

static ssize_t simple_char_read(struct file *filep, char __user *buffer, size_t len, loff_t *offset) {
    int bytes_read = 0;

    // Check if we are at the end of the buffer
    if (*offset >= buffer_len) {
        return 0; // End of file
    }

    // Determine how many bytes to read
    bytes_read = (len > buffer_len - *offset) ? (buffer_len - *offset) : len;
    
    // Copy data from kernel buffer to user buffer
    if (copy_to_user(buffer, device_buffer + *offset, bytes_read) != 0) {
        printk(KERN_ALERT "simple_char_dev: Failed to copy data to user.\n");
        return -EFAULT;
    }

    *offset += bytes_read;
    printk(KERN_INFO "simple_char_dev: Read %d bytes from device.\n", bytes_read);
    return bytes_read;
}

static ssize_t simple_char_write(struct file *filep, const char __user *buffer, size_t len, loff_t *offset) {
    // Check if the write operation will exceed the buffer size
    if (len > BUFFER_SIZE) {
        printk(KERN_ALERT "simple_char_dev: Write operation too large, max is %d bytes.\n", BUFFER_SIZE);
        return -EMSGSIZE;
    }

    // Copy data from user buffer to kernel buffer
    if (copy_from_user(device_buffer, buffer, len) != 0) {
        printk(KERN_ALERT "simple_char_dev: Failed to copy data from user.\n");
        return -EFAULT;
    }
    
    buffer_len = len;
    *offset = 0; // Reset offset for subsequent reads
    
    printk(KERN_INFO "simple_char_dev: Wrote %zu bytes to device.\n", len);
    return len;
}

static long simple_char_ioctl(struct file *filep, unsigned int cmd, unsigned long arg) {
    int32_t value_to_write;
    
    switch (cmd) {
        case WR_VALUE:
            // Receive an integer value from user space
            if (copy_from_user(&value_to_write, (int32_t*)arg, sizeof(value_to_write))) {
                return -EFAULT;
            }
            ioctl_value = value_to_write;
            printk(KERN_INFO "simple_char_dev: IOCTL: Setting value to %d\n", ioctl_value);
            break;
            
        case RD_VALUE:
            // Send the stored integer value to user space
            if (copy_to_user((int32_t*)arg, &ioctl_value, sizeof(ioctl_value))) {
                return -EFAULT;
            }
            printk(KERN_INFO "simple_char_dev: IOCTL: Reading value as %d\n", ioctl_value);
            break;
            
        default:
            printk(KERN_INFO "simple_char_dev: IOCTL command not recognized\n");
            return -ENOTTY;
    }
    
    return 0;
}

static int simple_char_release(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "simple_char_dev: Device closed.\n");
    return 0;
}

// File operations structure
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = simple_char_open,
    .read = simple_char_read,
    .write = simple_char_write,
    .unlocked_ioctl = simple_char_ioctl,
    .release = simple_char_release,
};

// Module initialization function
static int __init simple_char_init(void) {
    int ret;
    printk(KERN_INFO "simple_char_dev: Initializing the character device.\n");
    
    // Allocate a major number dynamically
    ret = alloc_chrdev_region(&simple_char_cdev.dev, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_ALERT "simple_char_dev: Failed to allocate major number.\n");
        return ret;
    }
    major_number = MAJOR(simple_char_cdev.dev);
    printk(KERN_INFO "simple_char_dev: Registered with major number %d and minor number %d\n", major_number, 0);

    // Initialize and add the cdev structure
    cdev_init(&simple_char_cdev, &fops);
    ret = cdev_add(&simple_char_cdev, simple_char_cdev.dev, 1);
    if (ret < 0) {
        unregister_chrdev_region(simple_char_cdev.dev, 1);
        printk(KERN_ALERT "simple_char_dev: Failed to add cdev.\n");
        return ret;
    }
    
    // Create device class
    simple_char_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(simple_char_class)) {
        cdev_del(&simple_char_cdev);
        unregister_chrdev_region(simple_char_cdev.dev, 1);
        printk(KERN_ALERT "simple_char_dev: Failed to register device class.\n");
        return PTR_ERR(simple_char_class);
    }

    // Create device file in /dev directory
    simple_char_device = device_create(simple_char_class, NULL, simple_char_cdev.dev, NULL, DEVICE_NAME);
    if (IS_ERR(simple_char_device)) {
        class_destroy(simple_char_class);
        cdev_del(&simple_char_cdev);
        unregister_chrdev_region(simple_char_cdev.dev, 1);
        printk(KERN_ALERT "simple_char_dev: Failed to create the device.\n");
        return PTR_ERR(simple_char_device);
    }
    
    // Allocate the buffer memory
    device_buffer = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    if (!device_buffer) {
        device_destroy(simple_char_class, simple_char_cdev.dev);
        class_destroy(simple_char_class);
        cdev_del(&simple_char_cdev);
        unregister_chrdev_region(simple_char_cdev.dev, 1);
        printk(KERN_ALERT "simple_char_dev: Failed to allocate device buffer.\n");
        return -ENOMEM;
    }
    memset(device_buffer, 0, BUFFER_SIZE);
    
    printk(KERN_INFO "simple_char_dev: Driver initialized successfully.\n");
    return 0;
}

// Module exit function
static void __exit simple_char_exit(void) {
    kfree(device_buffer);
    device_destroy(simple_char_class, simple_char_cdev.dev);
    class_unregister(simple_char_class);
    class_destroy(simple_char_class);
    cdev_del(&simple_char_cdev);
    unregister_chrdev_region(simple_char_cdev.dev, 1);
    printk(KERN_INFO "simple_char_dev: Module exited successfully.\n");
}

module_init(simple_char_init);
module_exit(simple_char_exit);
