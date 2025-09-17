#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>

#define DEVICE_NAME "simple_char_dev"
#define CLASS_NAME "simple_char_class"
#define BUFFER_SIZE 1024

// Module information
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple character device driver with read/write support.");
MODULE_VERSION("1.0");

// Global variables
static int major_number;
static struct class* simple_char_class = NULL;
static struct device* simple_char_device = NULL;
static struct cdev simple_char_cdev;
static char device_buffer[BUFFER_SIZE] = {0};
static int buffer_len = 0;

// Device file operations
static int simple_char_open(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "simple_char_dev: Device opened successfully\n");
    return 0;
}

static ssize_t simple_char_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
    int bytes_read = 0;
    
    // Check if we're at the end of the buffer
    if (*offset >= buffer_len) {
        return 0; // End of file
    }

    // Determine how many bytes to read
    if (len > buffer_len - *offset) {
        bytes_read = buffer_len - *offset;
    } else {
        bytes_read = len;
    }
    
    // Copy data from kernel space to user space
    if (copy_to_user(buffer, device_buffer + *offset, bytes_read) != 0) {
        printk(KERN_ALERT "simple_char_dev: Failed to copy data to user\n");
        return -EFAULT;
    }

    *offset += bytes_read;
    printk(KERN_INFO "simple_char_dev: Read %d bytes from device\n", bytes_read);
    return bytes_read;
}

static ssize_t simple_char_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
    int bytes_written = 0;

    // Check if the write operation will overflow the buffer
    if (len > BUFFER_SIZE) {
        printk(KERN_ALERT "simple_char_dev: Write operation too large, maximum is %d bytes\n", BUFFER_SIZE);
        return -EMSGSIZE;
    }
    
    // Copy data from user space to kernel space
    if (copy_from_user(device_buffer, buffer, len) != 0) {
        printk(KERN_ALERT "simple_char_dev: Failed to copy data from user\n");
        return -EFAULT;
    }
    
    buffer_len = len;
    bytes_written = len;

    printk(KERN_INFO "simple_char_dev: Wrote %d bytes to device\n", bytes_written);
    return bytes_written;
}

static int simple_char_release(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "simple_char_dev: Device successfully closed\n");
    return 0;
}

// File operations structure
static struct file_operations fops = {
    .open = simple_char_open,
    .read = simple_char_read,
    .write = simple_char_write,
    .release = simple_char_release,
};

// Module initialization
static int __init simple_char_init(void) {
    printk(KERN_INFO "simple_char_dev: Initializing the character device\n");
    
    // Dynamically allocate a major number
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        printk(KERN_ALERT "simple_char_dev: Failed to register a major number\n");
        return major_number;
    }
    printk(KERN_INFO "simple_char_dev: Registered with major number %d\n", major_number);
    
    // Register the device class
    simple_char_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(simple_char_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "simple_char_dev: Failed to register device class\n");
        return PTR_ERR(simple_char_class);
    }
    printk(KERN_INFO "simple_char_dev: Device class registered\n");

    // Create the device node
    simple_char_device = device_create(simple_char_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(simple_char_device)) {
        class_destroy(simple_char_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "simple_char_dev: Failed to create the device\n");
        return PTR_ERR(simple_char_device);
    }
    printk(KERN_INFO "simple_char_dev: Device created\n");
    
    cdev_init(&simple_char_cdev, &fops);
    cdev_add(&simple_char_cdev, MKDEV(major_number, 0), 1);
    
    return 0;
}

// Module exit
static void __exit simple_char_exit(void) {
    cdev_del(&simple_char_cdev);
    device_destroy(simple_char_class, MKDEV(major_number, 0));
    class_unregister(simple_char_class);
    class_destroy(simple_char_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    printk(KERN_INFO "simple_char_dev: Module exited successfully\n");
}

module_init(simple_char_init);
module_exit(simple_char_exit);