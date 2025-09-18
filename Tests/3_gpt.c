/*
 * simple_blkdev.c
 *
 * Minimal block device driver (virtual RAM disk) with:
 *  - block device registration
 *  - request handling via make_request_fn
 *  - basic read/write block operations
 *  - 1MB virtual disk size
 *
 * This is a learning example, not meant for production.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/blkdev.h>
#include <linux/blk_types.h>
#include <linux/hdreg.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/bio.h>
#include <linux/genhd.h>

#define DEVICE_NAME "simpleblk"
#define KERNEL_SECTOR_SIZE 512
#define DISK_SIZE_BYTES (1024 * 1024) /* 1 MB */
#define NSECTORS (DISK_SIZE_BYTES / KERNEL_SECTOR_SIZE)

static int major_num = 0;
module_param(major_num, int, 0444);
MODULE_PARM_DESC(major_num, "Major number (0 = dynamic)");

struct simple_blkdev {
    unsigned char *data;       /* vmalloc'd disk data */
    spinlock_t lock;          /* request queue lock */
    struct request_queue *queue;
    struct gendisk *gd;
};

static struct simple_blkdev *dev = NULL;

/* Helper: bounds check */
static inline int check_bounds(sector_t sector, unsigned int nbytes)
{
    unsigned long offset = sector * KERNEL_SECTOR_SIZE;
    if (offset + nbytes > DISK_SIZE_BYTES)
        return -EIO;
    return 0;
}

/* make_request function: handle BIOs */
static blk_status_t simple_make_request(struct request_queue *q, struct bio *bio)
{
    struct bio_vec bvec;
    struct bvec_iter iter;
    sector_t sector = bio->bi_iter.bi_sector; /* starting sector */
    int rw = bio_data_dir(bio); /* READ = 0, WRITE = 1 */

    bio_for_each_segment(bvec, bio, iter) {
        void *disk_mem;
        void *iovec_mem;
        unsigned int len = bvec.bv_len;
        unsigned long offset = sector * KERNEL_SECTOR_SIZE + bvec.bv_offset - bvec.bv_offset; /* simplified */
        /* Note: we'll compute offset differently below */

        /* bounds check */
        if (check_bounds(sector, len)) {
            bio_io_error(bio);
            return BLK_STS_IOERR;
        }

        /* Map page */
        iovec_mem = kmap_atomic(bvec.bv_page) + bvec.bv_offset;

        /* Compute disk memory pointer for this segment */
        disk_mem = dev->data + (sector * KERNEL_SECTOR_SIZE);

        if (rw == WRITE) {
            /* write from iovec -> disk */
            memcpy(disk_mem, iovec_mem, len);
        } else {
            /* read from disk -> iovec */
            memcpy(iovec_mem, disk_mem, len);
        }

        kunmap_atomic(iovec_mem - bvec.bv_offset);

        /* advance sector by the number of sectors consumed */
        sector += len / KERNEL_SECTOR_SIZE;
    }

    bio_endio(bio);
    return BLK_STS_OK;
}

static int simple_open(struct block_device *bdev, fmode_t mode)
{
    /* nothing special */
    return 0;
}

static void simple_release(struct gendisk *gd, fmode_t mode)
{
    /* nothing special */
}

static struct block_device_operations simple_ops = {
    .owner = THIS_MODULE,
    .open = simple_open,
    .release = simple_release,
};

static int __init simple_blk_init(void)
{
    int ret = 0;

    dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;

    /* allocate the in-memory disk */
    dev->data = vmalloc(DISK_SIZE_BYTES);
    if (!dev->data) {
        pr_err("simpleblk: vmalloc failed\n");
        ret = -ENOMEM;
        goto out_free_dev;
    }
    memset(dev->data, 0, DISK_SIZE_BYTES);

    spin_lock_init(&dev->lock);

    /* Register block device major */
    major_num = register_blkdev(major_num, DEVICE_NAME);
    if (major_num <= 0) {
        pr_err("simpleblk: register_blkdev failed\n");
        ret = -EBUSY;
        goto out_vfree;
    }

    /* Create request queue */
    dev->queue = blk_alloc_queue(GFP_KERNEL);
    if (!dev->queue) {
        pr_err("simpleblk: blk_alloc_queue failed\n");
        ret = -ENOMEM;
        goto out_unregister;
    }
    blk_queue_logical_block_size(dev->queue, KERNEL_SECTOR_SIZE);
    blk_queue_physical_block_size(dev->queue, KERNEL_SECTOR_SIZE);
    blk_queue_max_hw_sectors(dev->queue, 128);
    blk_queue_make_request(dev->queue, simple_make_request);

    /* Allocate gendisk */
    dev->gd = alloc_disk(1);
    if (!dev->gd) {
        pr_err("simpleblk: alloc_disk failed\n");
        ret = -ENOMEM;
        goto out_cleanup_queue;
    }

    dev->gd->major = major_num;
    dev->gd->first_minor = 0;
    dev->gd->fops = &simple_ops;
    dev->gd->queue = dev->queue;
    dev->gd->private_data = dev;
    snprintf(dev->gd->disk_name, DISK_NAME_LEN, DEVICE_NAME);
    set_capacity(dev->gd, NSECTORS);

    add_disk(dev->gd);

    pr_info("simpleblk: loaded (major=%d), size=%d bytes (%d sectors)\n",
            major_num, DISK_SIZE_BYTES, NSECTORS);
    return 0;

out_cleanup_queue:
    blk_cleanup_queue(dev->queue);
out_unregister:
    unregister_blkdev(major_num, DEVICE_NAME);
out_vfree:
    vfree(dev->data);
out_free_dev:
    kfree(dev);
    return ret;
}

static void __exit simple_blk_exit(void)
{
    if (!dev)
        return;

    del_gendisk(dev->gd);
    put_disk(dev->gd);
    blk_cleanup_queue(dev->queue);
    unregister_blkdev(major_num, DEVICE_NAME);
    vfree(dev->data);
    kfree(dev);
    pr_info("simpleblk: unloaded\n");
}

module_init(simple_blk_init);
module_exit(simple_blk_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ChatGPT");
MODULE_DESCRIPTION("Simple block device (1MB RAM disk) example");
MODULE_VERSION("0.1");
