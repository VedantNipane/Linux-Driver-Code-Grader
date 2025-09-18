#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/device.h>

#define DEVICE_NAME "mychardev"
#define CLASS_NAME "mychar"
#define BUFFER_SIZE 1024

// IOCTL command definitions
#define MYCHAR_IOC_MAGIC 'k'
#define MYCHAR_RESET     _IO(MYCHAR_IOC_MAGIC, 0)
#define MYCHAR_SET_SIZE  _IOW(MYCHAR_IOC_MAGIC, 1, int)
#define MYCHAR_GET_SIZE  _IOR(MYCHAR_IOC_MAGIC, 2, int)
#define MYCHAR_GET_STATS _IOR(MYCHAR_IOC_MAGIC, 3, struct device_stats)
#define MYCHAR_IOC_MAXNR 3

// Statistics structure
struct device_stats {
    unsigned long read_count;
    unsigned long write_count;
    unsigned long bytes_read;
    unsigned long bytes_written;
};

// Device structure
struct mychar_dev {
    struct cdev cdev;
    char *buffer;
    size_t buffer_size;
    size_t data_size;
    struct mutex mutex;
    struct device_stats stats;
};

// Global variables
static int major_number;
static struct class *mychar_class = NULL;
static struct device *mychar_device = NULL;
static struct mychar_dev *mychar_dev_ptr = NULL;

// Function prototypes
static int mychar_open(struct inode *, struct file *);
static int mychar_release(struct inode *, struct file *);
static ssize_t mychar_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t mychar_write(struct file *, const char __user *, size_t, loff_t *);
static long mychar_ioctl(struct file *, unsigned int, unsigned long);

// File operations structure
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = mychar_open,
    .release = mychar_release,
    .read = mychar_read,
    .write = mychar_write,
    .unlocked_ioctl = mychar_ioctl,
    .llseek = no_llseek,
};

// Open device file
static int mychar_open(struct inode *inodep, struct file *filep)
{
    struct mychar_dev *dev;
    
    printk(KERN_INFO "mychardev: Device opened\n");
    
    // Get the device structure
    dev = container_of(inodep->i_cdev, struct mychar_dev, cdev);
    filep->private_data = dev;
    
    return 0;
}

// Release device file
static int mychar_release(struct inode *inodep, struct file *filep)
{
    printk(KERN_INFO "mychardev: Device closed\n");
    return 0;
}

// Read from device
static ssize_t mychar_read(struct file *filep, char __user *buffer, size_t len, loff_t *offset)
{
    struct mychar_dev *dev = filep->private_data;
    ssize_t bytes_read = 0;
    int error_count = 0;
    
    if (!dev) {
        printk(KERN_ERR "mychardev: Invalid device pointer\n");
        return -ENODEV;
    }
    
    // Lock the device
    if (mutex_lock_interruptible(&dev->mutex))
        return -ERESTARTSYS;
    
    // Check if we're at end of data
    if (*offset >= dev->data_size) {
        mutex_unlock(&dev->mutex);
        return 0; // EOF
    }
    
    // Calculate bytes to read
    bytes_read = min(len, (size_t)(dev->data_size - *offset));
    
    // Copy data to user space
    error_count = copy_to_user(buffer, dev->buffer + *offset, bytes_read);
    if (error_count != 0) {
        printk(KERN_ERR "mychardev: Failed to copy %d bytes to user\n", error_count);
        bytes_read = -EFAULT;
        goto out;
    }
    
    // Update offset and statistics
    *offset += bytes_read;
    dev->stats.read_count++;
    dev->stats.bytes_read += bytes_read;
    
    printk(KERN_INFO "mychardev: Read %ld bytes from device\n", bytes_read);

out:
    mutex_unlock(&dev->mutex);
    return bytes_read;
}

// Write to device
static ssize_t mychar_write(struct file *filep, const char __user *buffer, size_t len, loff_t *offset)
{
    struct mychar_dev *dev = filep->private_data;
    ssize_t bytes_written = 0;
    int error_count = 0;
    
    if (!dev) {
        printk(KERN_ERR "mychardev: Invalid device pointer\n");
        return -ENODEV;
    }
    
    // Lock the device
    if (mutex_lock_interruptible(&dev->mutex))
        return -ERESTARTSYS;
    
    // Check if we can write data
    if (*offset >= dev->buffer_size) {
        mutex_unlock(&dev->mutex);
        return -ENOSPC; // No space left
    }
    
    // Calculate bytes to write
    bytes_written = min(len, (size_t)(dev->buffer_size - *offset));
    
    // Copy data from user space
    error_count = copy_from_user(dev->buffer + *offset, buffer, bytes_written);
    if (error_count != 0) {
        printk(KERN_ERR "mychardev: Failed to copy %d bytes from user\n", error_count);
        bytes_written = -EFAULT;
        goto out;
    }
    
    // Update data size, offset, and statistics
    *offset += bytes_written;
    if (*offset > dev->data_size)
        dev->data_size = *offset;
    
    dev->stats.write_count++;
    dev->stats.bytes_written += bytes_written;
    
    printk(KERN_INFO "mychardev: Wrote %ld bytes to device\n", bytes_written);

out:
    mutex_unlock(&dev->mutex);
    return bytes_written;
}

// IOCTL handler
static long mychar_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
    struct mychar_dev *dev = filep->private_data;
    int retval = 0;
    int size;
    struct device_stats stats;
    
    if (!dev) {
        printk(KERN_ERR "mychardev: Invalid device pointer\n");
        return -ENODEV;
    }
    
    // Check magic number and command range
    if (_IOC_TYPE(cmd) != MYCHAR_IOC_MAGIC)
        return -ENOTTY;
    if (_IOC_NR(cmd) > MYCHAR_IOC_MAXNR)
        return -ENOTTY;
    
    // Check access permissions
    if (_IOC_DIR(cmd) & _IOC_READ)
        retval = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
    else if (_IOC_DIR(cmd) & _IOC_WRITE)
        retval = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
    
    if (retval)
        return -EFAULT;
    
    // Lock the device
    if (mutex_lock_interruptible(&dev->mutex))
        return -ERESTARTSYS;
    
    switch (cmd) {
    case MYCHAR_RESET:
        printk(KERN_INFO "mychardev: Resetting device\n");
        memset(dev->buffer, 0, dev->buffer_size);
        dev->data_size = 0;
        memset(&dev->stats, 0, sizeof(struct device_stats));
        break;
        
    case MYCHAR_SET_SIZE:
        if (get_user(size, (int __user *)arg)) {
            retval = -EFAULT;
            break;
        }
        
        if (size <= 0 || size > 4096) {
            retval = -EINVAL;
            break;
        }
        
        // Reallocate buffer if needed
        if (size != dev->buffer_size) {
            char *new_buffer = krealloc(dev->buffer, size, GFP_KERNEL);
            if (!new_buffer) {
                retval = -ENOMEM;
                break;
            }
            
            dev->buffer = new_buffer;
            dev->buffer_size = size;
            if (dev->data_size > size)
                dev->data_size = size;
                
            printk(KERN_INFO "mychardev: Buffer size set to %d bytes\n", size);
        }
        break;
        
    case MYCHAR_GET_SIZE:
        if (put_user(dev->buffer_size, (int __user *)arg))
            retval = -EFAULT;
        break;
        
    case MYCHAR_GET_STATS:
        stats = dev->stats;
        if (copy_to_user((struct device_stats __user *)arg, &stats, sizeof(stats)))
            retval = -EFAULT;
        break;
        
    default:
        retval = -ENOTTY;
        break;
    }
    
    mutex_unlock(&dev->mutex);
    return retval;
}

// Module initialization
static int __init mychar_init(void)
{
    int result;
    dev_t dev_num;
    
    printk(KERN_INFO "mychardev: Initializing module\n");
    
    // Allocate device structure
    mychar_dev_ptr = kzalloc(sizeof(struct mychar_dev), GFP_KERNEL);
    if (!mychar_dev_ptr) {
        printk(KERN_ERR "mychardev: Failed to allocate device structure\n");
        return -ENOMEM;
    }
    
    // Initialize mutex
    mutex_init(&mychar_dev_ptr->mutex);
    
    // Allocate initial buffer
    mychar_dev_ptr->buffer = kzalloc(BUFFER_SIZE, GFP_KERNEL);
    if (!mychar_dev_ptr->buffer) {
        printk(KERN_ERR "mychardev: Failed to allocate buffer\n");
        result = -ENOMEM;
        goto fail_buffer;
    }
    mychar_dev_ptr->buffer_size = BUFFER_SIZE;
    mychar_dev_ptr->data_size = 0;
    
    // Allocate major number dynamically
    result = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (result < 0) {
        printk(KERN_ERR "mychardev: Failed to allocate major number\n");
        goto fail_chrdev;
    }
    major_number = MAJOR(dev_num);
    
    // Initialize and add character device
    cdev_init(&mychar_dev_ptr->cdev, &fops);
    mychar_dev_ptr->cdev.owner = THIS_MODULE;
    
    result = cdev_add(&mychar_dev_ptr->cdev, dev_num, 1);
    if (result) {
        printk(KERN_ERR "mychardev: Failed to add cdev\n");
        goto fail_cdev_add;
    }
    
    // Create device class
    mychar_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(mychar_class)) {
        printk(KERN_ERR "mychardev: Failed to create class\n");
        result = PTR_ERR(mychar_class);
        goto fail_class;
    }
    
    // Create device
    mychar_device = device_create(mychar_class, NULL, dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(mychar_device)) {
        printk(KERN_ERR "mychardev: Failed to create device\n");
        result = PTR_ERR(mychar_device);
        goto fail_device;
    }
    
    printk(KERN_INFO "mychardev: Module loaded successfully (Major: %d)\n", major_number);
    return 0;
    
fail_device:
    class_destroy(mychar_class);
fail_class:
    cdev_del(&mychar_dev_ptr->cdev);
fail_cdev_add:
    unregister_chrdev_region(MKDEV(major_number, 0), 1);
fail_chrdev:
    kfree(mychar_dev_ptr->buffer);
fail_buffer:
    kfree(mychar_dev_ptr);
    return result;
}

// Module cleanup
static void __exit mychar_exit(void)
{
    printk(KERN_INFO "mychardev: Cleaning up module\n");
    
    // Remove device and class
    if (mychar_device)
        device_destroy(mychar_class, MKDEV(major_number, 0));
    if (mychar_class)
        class_destroy(mychar_class);
    
    // Remove character device
    if (mychar_dev_ptr) {
        cdev_del(&mychar_dev_ptr->cdev);
        kfree(mychar_dev_ptr->buffer);
        kfree(mychar_dev_ptr);
    }
    
    // Unregister major number
    unregister_chrdev_region(MKDEV(major_number, 0), 1);
    
    printk(KERN_INFO "mychardev: Module unloaded\n");
}

module_init(mychar_init);
module_exit(mychar_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A complete Linux character device driver");
MODULE_VERSION("1.0");