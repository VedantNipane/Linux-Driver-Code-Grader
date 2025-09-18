#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>
#include <linux/bio.h>

#define DEVICE_NAME "myblockdev"
#define KERNEL_SECTOR_SIZE 512
#define DISK_SIZE_MB 1
#define DISK_SIZE (DISK_SIZE_MB * 1024 * 1024)  // 1MB in bytes
#define NSECTORS (DISK_SIZE / KERNEL_SECTOR_SIZE) // Number of sectors

// Device structure
struct myblock_dev {
    unsigned long size;              // Device size in bytes
    spinlock_t lock;                // For mutual exclusion
    u8 *data;                       // The virtual disk data
    struct gendisk *gd;             // The gendisk structure
    struct request_queue *queue;    // The device request queue
    int major;                      // Major number
};

static struct myblock_dev *myblock_device;

// Forward declarations
static int myblock_open(struct block_device *bdev, fmode_t mode);
static void myblock_release(struct gendisk *gd, fmode_t mode);
static int myblock_getgeo(struct block_device *bdev, struct hd_geometry *geo);
static blk_qc_t myblock_make_request(struct request_queue *q, struct bio *bio);

// Block device operations
static struct block_device_operations myblock_fops = {
    .owner = THIS_MODULE,
    .open = myblock_open,
    .release = myblock_release,
    .getgeo = myblock_getgeo,
};

// Open the device
static int myblock_open(struct block_device *bdev, fmode_t mode)
{
    printk(KERN_INFO "myblockdev: Device opened\n");
    return 0;
}

// Release the device
static void myblock_release(struct gendisk *gd, fmode_t mode)
{
    printk(KERN_INFO "myblockdev: Device released\n");
}

// Get device geometry
static int myblock_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
    long size;
    
    // Get size in sectors
    size = myblock_device->size / KERNEL_SECTOR_SIZE;
    
    // Set up fake geometry
    geo->cylinders = (size & ~0x3f) >> 6;  // Number of cylinders
    geo->heads = 4;                        // Number of heads
    geo->sectors = 16;                     // Sectors per track
    geo->start = 0;                        // Start sector
    
    return 0;
}

// Transfer a single BIO
static int myblock_transfer_bio(struct myblock_dev *dev, struct bio *bio)
{
    struct bio_vec bvec;
    struct bvec_iter iter;
    sector_t sector = bio->bi_iter.bi_sector;
    char *buffer;
    unsigned long offset = sector * KERNEL_SECTOR_SIZE;
    int dir = bio_data_dir(bio);
    
    // Check for beyond-end-of-device
    if ((offset + bio->bi_iter.bi_size) > dev->size) {
        printk(KERN_ERR "myblockdev: Beyond-end write (%ld %u)\n",
               offset, bio->bi_iter.bi_size);
        return -EIO;
    }
    
    // Process each segment in the bio
    bio_for_each_segment(bvec, bio, iter) {
        buffer = kmap_atomic(bvec.bv_page);
        
        if (dir == WRITE) {
            // Write operation
            memcpy(dev->data + offset, 
                   buffer + bvec.bv_offset, 
                   bvec.bv_len);
            printk(KERN_DEBUG "myblockdev: Write at offset %ld, len %u\n",
                   offset, bvec.bv_len);
        } else {
            // Read operation
            memcpy(buffer + bvec.bv_offset, 
                   dev->data + offset, 
                   bvec.bv_len);
            printk(KERN_DEBUG "myblockdev: Read at offset %ld, len %u\n",
                   offset, bvec.bv_len);
        }
        
        kunmap_atomic(buffer);
        offset += bvec.bv_len;
    }
    
    return 0;
}

// Make request function - handles BIO requests directly
static blk_qc_t myblock_make_request(struct request_queue *q, struct bio *bio)
{
    struct myblock_dev *dev = q->queuedata;
    int status;
    
    // Transfer the bio
    spin_lock(&dev->lock);
    status = myblock_transfer_bio(dev, bio);
    spin_unlock(&dev->lock);
    
    // Complete the bio
    bio->bi_status = (status == 0) ? BLK_STS_OK : BLK_STS_IOERR;
    bio_endio(bio);
    
    return BLK_QC_T_NONE;
}

// Alternative request handler using request-based approach
static void myblock_request(struct request_queue *q)
{
    struct request *req;
    struct myblock_dev *dev = q->queuedata;
    struct bio *bio;
    int status = 0;
    
    while ((req = blk_fetch_request(q)) != NULL) {
        // Check if it's a filesystem request
        if (!blk_rq_is_passthrough(req)) {
            // Process each bio in the request
            __rq_for_each_bio(bio, req) {
                status = myblock_transfer_bio(dev, bio);
                if (status != 0)
                    break;
            }
        } else {
            printk(KERN_NOTICE "myblockdev: Skip non-fs request\n");
            status = -EIO;
        }
        
        // End the request
        __blk_end_request_all(req, status == 0 ? BLK_STS_OK : BLK_STS_IOERR);
    }
}

// Initialize the device
static int myblock_init_device(void)
{
    int ret;
    
    // Allocate device structure
    myblock_device = kzalloc(sizeof(struct myblock_dev), GFP_KERNEL);
    if (!myblock_device) {
        printk(KERN_ERR "myblockdev: Failed to allocate device structure\n");
        return -ENOMEM;
    }
    
    // Initialize device parameters
    myblock_device->size = DISK_SIZE;
    spin_lock_init(&myblock_device->lock);
    
    // Allocate the virtual disk storage
    myblock_device->data = vmalloc(myblock_device->size);
    if (!myblock_device->data) {
        printk(KERN_ERR "myblockdev: Failed to allocate disk storage\n");
        ret = -ENOMEM;
        goto fail_data;
    }
    memset(myblock_device->data, 0, myblock_device->size);
    
    // Get a major number
    myblock_device->major = register_blkdev(0, DEVICE_NAME);
    if (myblock_device->major < 0) {
        printk(KERN_ERR "myblockdev: Failed to get major number\n");
        ret = myblock_device->major;
        goto fail_major;
    }
    
    // Allocate and initialize request queue
    myblock_device->queue = blk_alloc_queue(GFP_KERNEL);
    if (!myblock_device->queue) {
        printk(KERN_ERR "myblockdev: Failed to allocate request queue\n");
        ret = -ENOMEM;
        goto fail_queue;
    }
    
    // Set up the queue - using make_request for direct bio handling
    blk_queue_make_request(myblock_device->queue, myblock_make_request);
    
    // Alternative: use request-based approach
    // blk_init_queue(myblock_request, &myblock_device->lock);
    
    // Set queue properties
    blk_queue_logical_block_size(myblock_device->queue, KERNEL_SECTOR_SIZE);
    myblock_device->queue->queuedata = myblock_device;
    
    // Allocate gendisk structure
    myblock_device->gd = alloc_disk(1);  // 1 minor number (no partitions)
    if (!myblock_device->gd) {
        printk(KERN_ERR "myblockdev: Failed to allocate gendisk\n");
        ret = -ENOMEM;
        goto fail_disk;
    }
    
    // Set up gendisk
    myblock_device->gd->major = myblock_device->major;
    myblock_device->gd->first_minor = 0;
    myblock_device->gd->fops = &myblock_fops;
    myblock_device->gd->private_data = myblock_device;
    myblock_device->gd->queue = myblock_device->queue;
    strcpy(myblock_device->gd->disk_name, DEVICE_NAME);
    set_capacity(myblock_device->gd, NSECTORS);
    
    // Make the disk available
    add_disk(myblock_device->gd);
    
    printk(KERN_INFO "myblockdev: Block device registered (Major: %d, Size: %lu bytes)\n",
           myblock_device->major, myblock_device->size);
    
    return 0;
    
fail_disk:
    blk_cleanup_queue(myblock_device->queue);
fail_queue:
    unregister_blkdev(myblock_device->major, DEVICE_NAME);
fail_major:
    vfree(myblock_device->data);
fail_data:
    kfree(myblock_device);
    return ret;
}

// Cleanup the device
static void myblock_cleanup_device(void)
{
    if (myblock_device) {
        if (myblock_device->gd) {
            del_gendisk(myblock_device->gd);
            put_disk(myblock_device->gd);
        }
        
        if (myblock_device->queue) {
            blk_cleanup_queue(myblock_device->queue);
        }
        
        if (myblock_device->major > 0) {
            unregister_blkdev(myblock_device->major, DEVICE_NAME);
        }
        
        if (myblock_device->data) {
            vfree(myblock_device->data);
        }
        
        kfree(myblock_device);
    }
}

// Module initialization
static int __init myblock_init(void)
{
    int ret;
    
    printk(KERN_INFO "myblockdev: Initializing block device driver\n");
    
    ret = myblock_init_device();
    if (ret) {
        printk(KERN_ERR "myblockdev: Failed to initialize device\n");
        return ret;
    }
    
    printk(KERN_INFO "myblockdev: Module loaded successfully\n");
    printk(KERN_INFO "myblockdev: Virtual disk size: %d MB (%lu sectors)\n",
           DISK_SIZE_MB, NSECTORS);
    
    return 0;
}

// Module cleanup
static void __exit myblock_exit(void)
{
    printk(KERN_INFO "myblockdev: Cleaning up block device driver\n");
    
    myblock_cleanup_device();
    
    printk(KERN_INFO "myblockdev: Module unloaded\n");
}

module_init(myblock_init);
module_exit(myblock_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple Linux block device driver with 1MB virtual disk");
MODULE_VERSION("1.0");