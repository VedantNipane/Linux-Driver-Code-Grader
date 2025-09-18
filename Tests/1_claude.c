#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>

#define DEVICE_NAME "simple_char"
#define CLASS_NAME "simple"
#define BUFFER_SIZE 1024

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Driver Developer");
MODULE_DESCRIPTION("A simple character device driver with 1KB buffer");
MODULE_VERSION("1.0");

static int major_number;
static char device_buffer[BUFFER_SIZE];
static int buffer_position = 0;
static int buffer_size = 0;
static struct class* simple_char_class = NULL;
static struct device* simple_char_device = NULL;
static DEFINE_MUTEX(simple_char_mutex);

// Function prototypes
static int dev_open(struct inode*, struct file*);
static int dev_release(struct inode*, struct file*);
static ssize_t dev_read(struct file*, char*, size_t, loff_t*);
static ssize_t dev_write(struct file*, const char*, size_t, loff_t*);

// File operations structure
static struct file_operations fops = {
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};

// Device open function
static int dev_open(struct inode *inodep, struct file *filep) {
    if (!mutex_trylock(&simple_char_mutex)) {
        printk(KERN_ALERT "simple_char: Device in use by another process\n");
        return -EBUSY;
    }
    printk(KERN_INFO "simple_char: Device opened successfully\n");
    return 0;
}

// Device read function
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
    int error_count = 0;
    int bytes_read = 0;
    
    // Check if there's data to read
    if (buffer_position >= buffer_size) {
        printk(KERN_INFO "simple_char: End of buffer reached\n");
        return 0; // EOF
    }
    
    // Calculate how many bytes to read
    bytes_read = (len < (buffer_size - buffer_position)) ? len : (buffer_size - buffer_position);
    
    // Copy data to user space
    error_count = copy_to_user(buffer, device_buffer + buffer_position, bytes_read);
    
    if (error_count == 0) {
        buffer_position += bytes_read;
        printk(KERN_INFO "simple_char: Read %d bytes from device\n", bytes_read);
        return bytes_read;
    } else {
        printk(KERN_INFO "simple_char: Failed to read %d bytes from device\n", error_count);
        return -EFAULT;
    }
}

// Device write function
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
    int bytes_to_write;
    int error_count = 0;
    
    // Reset buffer position for new write
    buffer_position = 0;
    
    // Calculate how many bytes we can write
    bytes_to_write = (len < BUFFER_SIZE) ? len : BUFFER_SIZE;
    
    // Copy data from user space
    error_count = copy_from_user(device_buffer, buffer, bytes_to_write);
    
    if (error_count == 0) {
        buffer_size = bytes_to_write;
        printk(KERN_INFO "simple_char: Wrote %d bytes to device\n", bytes_to_write);
        return bytes_to_write;
    } else {
        printk(KERN_INFO "simple_char: Failed to write %d bytes to device\n", error_count);
        return -EFAULT;
    }
}

// Device release function
static int dev_release(struct inode *inodep, struct file *filep) {
    mutex_unlock(&simple_char_mutex);
    printk(KERN_INFO "simple_char: Device successfully closed\n");
    return 0;
}

// Module initialization function
static int __init simple_char_init(void) {
    printk(KERN_INFO "simple_char: Initializing the device driver\n");
    
    // Allocate a major number for the device
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        printk(KERN_ALERT "simple_char: Failed to register a major number\n");
        return major_number;
    }
    printk(KERN_INFO "simple_char: Registered with major number %d\n", major_number);
    
    // Register the device class
    simple_char_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(simple_char_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "simple_char: Failed to register device class\n");
        return PTR_ERR(simple_char_class);
    }
    printk(KERN_INFO "simple_char: Device class registered correctly\n");
    
    // Register the device driver
    simple_char_device = device_create(simple_char_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(simple_char_device)) {
        class_destroy(simple_char_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "simple_char: Failed to create the device\n");
        return PTR_ERR(simple_char_device);
    }
    
    // Initialize buffer
    memset(device_buffer, 0, BUFFER_SIZE);
    buffer_position = 0;
    buffer_size = 0;
    
    printk(KERN_INFO "simple_char: Device driver initialized successfully\n");
    return 0;
}

// Module cleanup function
static void __exit simple_char_exit(void) {
    device_destroy(simple_char_class, MKDEV(major_number, 0));
    class_unregister(simple_char_class);
    class_destroy(simple_char_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    printk(KERN_INFO "simple_char: Device driver unloaded successfully\n");
}

module_init(simple_char_init);
module_exit(simple_char_exit);