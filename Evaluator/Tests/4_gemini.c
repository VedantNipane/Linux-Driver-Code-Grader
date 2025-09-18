#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/vmalloc.h>

// Module information
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple virtual network device driver.");
MODULE_VERSION("1.0");

// A structure to hold our device-specific data
struct net_private_data {
    struct net_device_stats stats;
    spinlock_t lock;
};

// Transmit function: sends a packet out
static netdev_tx_t simple_net_xmit(struct sk_buff *skb, struct net_device *dev) {
    struct net_private_data *priv = netdev_priv(dev);

    // Lock the private data for thread-safe access to statistics
    spin_lock(&priv->lock);
    
    // Increment transmit statistics
    priv->stats.tx_packets++;
    priv->stats.tx_bytes += skb->len;

    // Simulate sending the packet by simply dropping it.
    // In a real driver, you would write the packet to hardware.
    // To simulate a loopback device, you could reinject the packet.
    printk(KERN_INFO "simple_net: Transmitting a packet of size %d.\n", skb->len);

    // Free the sk_buff
    dev_kfree_skb(skb);

    // Unlock the private data
    spin_unlock(&priv->lock);

    return NETDEV_TX_OK;
}

// Function to get the network statistics
static struct net_device_stats *simple_net_stats(struct net_device *dev) {
    return &((struct net_private_data *)netdev_priv(dev))->stats;
}

// Net device operations structure
static const struct net_device_ops simple_net_ops = {
    .ndo_open = NULL,             // Open function (optional for this simple driver)
    .ndo_stop = NULL,             // Stop function (optional)
    .ndo_start_xmit = simple_net_xmit, // Transmit function
    .ndo_get_stats = simple_net_stats, // Get statistics function
};

// Initialization function for the network device
static void simple_net_setup(struct net_device *dev) {
    // Initialize the private data structure
    struct net_private_data *priv = netdev_priv(dev);
    memset(priv, 0, sizeof(struct net_private_data));
    spin_lock_init(&priv->lock);

    // Set the device operations
    dev->netdev_ops = &simple_net_ops;

    // Assign the Ethernet header size
    dev->hard_header_len = ETH_HLEN;

    // Set the MTU to the default Ethernet MTU
    dev->mtu = 1500;

    // Set the device flags
    dev->flags = IFF_NOARP | IFF_POINTOPOINT;

    // Set the device type
    dev->type = ARPHRD_ETHER;

    // Assign a default MAC address
    // This is a dummy address, you could use a hardware-assigned one.
    eth_hw_addr_set(dev, "\x00\x11\x22\x33\x44\x55");
}

// Device creation function
static struct net_device *simple_net_create(void) {
    struct net_device *dev;

    // Allocate the net_device structure. `alloc_netdev` is preferred.
    // The second argument is the size of the private data area.
    dev = alloc_netdev(sizeof(struct net_private_data), "veth%d", NET_NAME_ENUM, simple_net_setup);
    
    if (!dev) {
        printk(KERN_ALERT "simple_net: Failed to allocate net device.\n");
        return NULL;
    }

    return dev;
}

static struct net_device *my_net_device;

// Module initialization
static int __init simple_net_init(void) {
    int result;

    printk(KERN_INFO "simple_net: Initializing virtual network device driver.\n");
    
    // Create the network device instance
    my_net_device = simple_net_create();
    if (!my_net_device) {
        return -ENOMEM;
    }

    // Register the network device with the kernel
    result = register_netdev(my_net_device);
    if (result < 0) {
        printk(KERN_ALERT "simple_net: Failed to register network device.\n");
        free_netdev(my_net_device);
        return result;
    }

    printk(KERN_INFO "simple_net: Device '%s' registered successfully.\n", my_net_device->name);
    printk(KERN_INFO "simple_net: To use this device, bring it up with 'sudo ifconfig %s up'.\n", my_net_device->name);
    printk(KERN_INFO "simple_net: To see packets, use 'tcpdump -i %s'.\n", my_net_device->name);

    return 0;
}

// Module exit
static void __exit simple_net_exit(void) {
    if (my_net_device) {
        printk(KERN_INFO "simple_net: Unregistering and freeing device '%s'.\n", my_net_device->name);
        unregister_netdev(my_net_device);
        free_netdev(my_net_device);
    }
    printk(KERN_INFO "simple_net: Module exited successfully.\n");
}

module_init(simple_net_init);
module_exit(simple_net_exit);
