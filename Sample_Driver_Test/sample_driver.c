#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

static int __init sample_init(void) {
    printk(KERN_INFO "Sample driver loaded!\n");
    return 0;
}

static void __exit sample_exit(void) {
    printk(KERN_INFO "Sample driver unloaded!\n");
}

module_init(sample_init);
module_exit(sample_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple sample driver");
