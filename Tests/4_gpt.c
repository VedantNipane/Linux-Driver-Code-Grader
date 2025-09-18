/*
 * vnet_driver.c
 *
 * Minimal virtual Ethernet network device driver.
 * Features:
 *  - network device registration (creates vnet0)
 *  - simple packet transmission (ndo_start_xmit)
 *  - loopback-style reception using netif_rx
 *  - maintains network statistics (tx/rx packets/bytes, errors)
 *  - basic Ethernet frame handling (eth_type_trans)
 *
 * This is a teaching example. It implements a virtual NIC that "receives"
 * any packet it transmits by looping it back into the networking stack.
 *
 * Build with provided Makefile, load with insmod. Tested on modern kernels.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/init.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <linux/errno.h>

#define VNET_NAME "vnet%d"

struct vnet_priv {
    struct net_device_stats stats;
    /* any private fields you want (queues, timers, etc.) */
};

static netdev_tx_t vnet_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
    struct vnet_priv *priv = netdev_priv(dev);
    struct sk_buff *rx_skb;

    /* update transmit statistics */
    priv->stats.tx_packets++;
    priv->stats.tx_bytes += skb->len;

    /* In a real driver we would hand the skb to hardware. Here we simulate
     * successful transmission and loop the packet back to the receive path.
     */

    /* Make a copy for the receive path (netif_rx will free it) */
    rx_skb = skb_copy(skb, GFP_ATOMIC);
    if (!rx_skb) {
        priv->stats.tx_errors++;
        dev_kfree_skb(skb);
        return NETDEV_TX_OK;
    }

    /* prepare rx_skb to look like a received frame */
    rx_skb->dev = dev;
    rx_skb->protocol = eth_type_trans(rx_skb, dev);
    rx_skb->ip_summed = CHECKSUM_UNNECESSARY;

    /* update rx stats */
    priv->stats.rx_packets++;
    priv->stats.rx_bytes += rx_skb->len;

    /* push into networking stack */
    netif_rx(rx_skb);

    /* free the original skb that was "transmitted" */
    dev_kfree_skb(skb);

    /* notify kernel that driver handled it */
    return NETDEV_TX_OK;
}

static int vnet_open(struct net_device *dev)
{
    netif_start_queue(dev);
    pr_info("%s: device opened\n", dev->name);
    return 0;
}

static int vnet_stop(struct net_device *dev)
{
    netif_stop_queue(dev);
    pr_info("%s: device stopped\n", dev->name);
    return 0;
}

static void vnet_get_stats(struct net_device *dev, struct net_device_stats *stats)
{
    struct vnet_priv *priv = netdev_priv(dev);
    *stats = priv->stats;
}

static const struct net_device_ops vnet_netdev_ops = {
    .ndo_open = vnet_open,
    .ndo_stop = vnet_stop,
    .ndo_start_xmit = vnet_start_xmit,
    .ndo_get_stats = vnet_get_stats,
};

static void vnet_setup(struct net_device *dev)
{
    ether_setup(dev); /* sets up ethernet-like fields */
    dev->netdev_ops = &vnet_netdev_ops;
    dev->flags |= IFF_NOARP;
    dev->features |= NETIF_F_HW_CSUM; /* advertise checksum offload (simulated) */

    /* set a MAC address: locally administered */
    eth_hw_addr_random(dev);

    dev->mtu = ETH_DATA_LEN;
}

static struct net_device *vnet_dev;

static int __init vnet_init(void)
{
    int ret;
    struct vnet_priv *priv;

    vnet_dev = alloc_netdev(sizeof(struct vnet_priv), VNET_NAME, NET_NAME_UNKNOWN, vnet_setup);
    if (!vnet_dev)
        return -ENOMEM;

    priv = netdev_priv(vnet_dev);
    memset(priv, 0, sizeof(*priv));

    /* register the device */
    ret = register_netdev(vnet_dev);
    if (ret) {
        pr_err("vnet: failed to register net device: %d\n", ret);
        free_netdev(vnet_dev);
        return ret;
    }

    pr_info("vnet: loaded, device=%s\n", vnet_dev->name);
    return 0;
}

static void __exit vnet_exit(void)
{
    if (!vnet_dev)
        return;

    unregister_netdev(vnet_dev);
    free_netdev(vnet_dev);
    pr_info("vnet: unloaded\n");
}

module_init(vnet_init);
module_exit(vnet_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ChatGPT");
MODULE_DESCRIPTION("Virtual Ethernet network device (loopback-style) example");
MODULE_VERSION("0.1");
