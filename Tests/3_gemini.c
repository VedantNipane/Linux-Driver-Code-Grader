#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/spinlock.h>
#include <linux/genhd.h>
#include <linux/vmalloc.h>
#include <linux/hdreg.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple block device driver with a 1MB virtual disk.");
MODULE_VERSION("1.0");

// Macros for the virtual disk
#define VIRT_DISK_NAME "vblock"
#define VIRT_DISK_MINORS 1
#define VIRT_DISK_SIZE_MB 1
#define VIRT_DISK_SECTOR_SIZE 512
#define VIRT_DISK_SECTORS (VIRT_DISK_SIZE_MB * 1024 * 1024 / VIRT_DISK_SECTOR_SIZE)

// Structure to hold the device-specific information
static struct simple_bd {
    // The request queue, handling all I/O requests for the device
    struct request_queue *queue;

    // The gendisk structure, representing the block device to the kernel
    struct gendisk *disk;

    // The in-memory buffer acting as our virtual disk
    u8 *data;

    // Spinlock for thread-safe access to the data
    spinlock_t lock;
} simple_bd_device;

// Block device operations
static struct block_device_operations fops = {
    .owner = THIS_MODULE,
};

// Function to handle I/O requests from the kernel
static blk_qc_t simple_bd_request(struct request_queue *q, struct bio *bio) {
    struct bio_vec bvec;
    struct bvec_iter iter;
    unsigned long sector = bio->bi_iter.bi_sector;
    u8 *data_ptr = simple_bd_device.data + (sector * VIRT_DISK_SECTOR_SIZE);

    // The bio structure is a list of segments. We must iterate through them.
    bio_for_each_segment(bvec, bio, iter) {
        // Map the page to kernel space
        void *page_data = kmap(bvec.bv_page);
        
        // Calculate the physical address and length for this segment
        unsigned int len = bvec.bv_len;
        unsigned int offset = bvec.bv_offset;

        // Perform the read or write operation
        if (bio_data_dir(bio) == REQ_OP_WRITE) {
            // Write from the page to our disk data buffer
            memcpy(data_ptr, page_data + offset, len);
        } else {
            // Read from our disk data buffer to the page
            memcpy(page_data + offset, data_ptr, len);
        }

        // Unmap the page
        kunmap(bvec.bv_page);
        
        // Move the data pointer forward
        data_ptr += len;
    }

    // Mark the bio as complete
    bio_endio(bio);

    return BLK_QC_T_NONE;
}

// Module initialization function
static int __init simple_bd_init(void) {
    int ret = 0;

    printk(KERN_INFO "simple_bd: Initializing block device driver.\n");

    // 1. Allocate a virtual memory buffer for the disk
    simple_bd_device.data = vmalloc(VIRT_DISK_SIZE_MB * 1024 * 1024);
    if (!simple_bd_device.data) {
        printk(KERN_ALERT "simple_bd: Failed to allocate memory for virtual disk.\n");
        return -ENOMEM;
    }

    // 2. Initialize the spinlock for the queue
    spin_lock_init(&simple_bd_device.lock);

    // 3. Allocate a request queue
    simple_bd_device.queue = blk_mq_init_sq_queue(&simple_bd_device.lock, simple_bd_request, 128, BLK_MQ_F_SHOULD_MERGE);
    if (IS_ERR(simple_bd_device.queue)) {
        ret = PTR_ERR(simple_bd_device.queue);
        vfree(simple_bd_device.data);
        printk(KERN_ALERT "simple_bd: Failed to allocate request queue.\n");
        return ret;
    }

    // 4. Allocate and set up the gendisk structure
    simple_bd_device.disk = alloc_disk(VIRT_DISK_MINORS);
    if (!simple_bd_device.disk) {
        blk_cleanup_queue(simple_bd_device.queue);
        vfree(simple_bd_device.data);
        printk(KERN_ALERT "simple_bd: Failed to allocate gendisk structure.\n");
        return -ENOMEM;
    }

    // Assign the queue to the disk
    simple_bd_device.disk->queue = simple_bd_device.queue;
    simple_bd_device.disk->fops = &fops;
    simple_bd_device.disk->private_data = &simple_bd_device;
    snprintf(simple_bd_device.disk->disk_name, DISK_NAME_LEN, VIRT_DISK_NAME "d");

    // Set the number of sectors (disk size)
    set_capacity(simple_bd_device.disk, VIRT_DISK_SECTORS);

    // 5. Add the disk to the kernel
    add_disk(simple_bd_device.disk);

    printk(KERN_INFO "simple_bd: Block device initialized and ready. Size is %dMB.\n", VIRT_DISK_SIZE_MB);
    return 0;
}

// Module exit function
static void __exit simple_bd_exit(void) {
    printk(KERN_INFO "simple_bd: Exiting block device driver.\n");

    // 1. Delete the disk from the kernel
    if (simple_bd_device.disk) {
        del_gendisk(simple_bd_device.disk);
    }
    
    // 2. Clean up the request queue
    if (simple_bd_device.queue) {
        blk_cleanup_queue(simple_bd_device.queue);
    }
    
    // 3. Free the allocated virtual disk memory
    if (simple_bd_device.data) {
        vfree(simple_bd_device.data);
    }
}

module_init(simple_bd_init);
module_exit(simple_bd_exit);
