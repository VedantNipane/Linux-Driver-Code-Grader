/*
 * hello_driver.c - The simplest possible Linux device driver
 * This driver does almost nothing - just loads and unloads
 */

#include <linux/init.h>      // For __init and __exit macros
#include <linux/module.h>    // For all module-related functions
#include <linux/kernel.h>    // For printk()

/* This function is called when the driver is loaded */
static int __init hello_driver_init(void)
{
    printk(KERN_INFO "Hello! My simple driver is loaded\n");
    return 0;  // 0 means success
}

/* This function is called when the driver is unloaded */
static void __exit hello_driver_exit(void)
{
    printk(KERN_INFO "Goodbye! My simple driver is being removed\n");
}

/* Tell the kernel which functions to call when loading/unloading */
module_init(hello_driver_init);
module_exit(hello_driver_exit);

/* Module information */
MODULE_LICENSE("GPL");              // License type
MODULE_AUTHOR("Your Name");         // Who wrote this
MODULE_DESCRIPTION("A simple hello world driver");  // What it does
MODULE_VERSION("1.0");             // Version number