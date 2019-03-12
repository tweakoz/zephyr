/*
 * Copyright (c) 2019 Antmicro <www.antmicro.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <device.h>
#include <soc.h>
#include <stdbool.h>
#include <net/ethernet.h>
#include <net/net_if.h>
#include <net/net_pkt.h>
#include <zephyr/types.h>

/* inteface name */
#define LITEETH_IF_NAME CONFIG_LITEETH_ETH_0_NAME

/* flags */
#define LITEETH_EV_TX 0x1
#define LITEETH_EV_RX 0x1

/* slots */
#define LITEETH_SLOT_BASE   CONFIG_LITEETH_ETH_0_BASE_SLOT_ADDR
#define LITEETH_SLOT_RX0    ((LITEETH_SLOT_BASE) + 0x0000) 
#define LITEETH_SLOT_RX1    ((LITEETH_SLOT_BASE) + 0x0800) 
#define LITEETH_SLOT_TX0    ((LITEETH_SLOT_BASE) + 0x1000) 
#define LITEETH_SLOT_TX1    ((LITEETH_SLOT_BASE) + 0x1800) 

/* sram - rx */
#define LITEETH_RX_BASE         CONFIG_LITEETH_ETH_0_BASE_SRAM_ADDR
#define LITEETH_RX_SLOT         ((LITEETH_RX_BASE) + 0x00)
#define LITEETH_RX_LENGTH       ((LITEETH_RX_BASE) + 0x04)
#define LITEETH_RX_EV_PENDING   ((LITEETH_RX_BASE) + 0x28)
#define LITEETH_RX_EV_ENABLE    ((LITEETH_RX_BASE) + 0x2c)

/* sram - tx */
#define LITEETH_TX_BASE         ((LITEETH_RX_BASE) + 0x30)
#define LITEETH_TX_START        ((LITEETH_TX_BASE) + 0x00)
#define LITEETH_TX_READY        ((LITEETH_TX_BASE) + 0x04)
#define LITEETH_TX_SLOT         ((LITEETH_TX_BASE) + 0x0c)
#define LITEETH_TX_LENGTH       ((LITEETH_TX_BASE) + 0x10)
#define LITEETH_TX_EV_PENDING   ((LITEETH_TX_BASE) + 0x1c)

/* irq */
#define LITEETH_IRQ CONFIG_LITEETH_ETH_0_IRQ

struct eth_liteeth_dev_data {
    struct net_if *iface;
    u8_t mac_addr[6];
};

struct eth_liteeth_config {
    void (*config_func)(void);
};

static u8_t txslot;
static u8_t rxslot;

static u8_t *txbuf;
static u8_t *txbuf0;
static u8_t *txbuf1;

static u8_t *rxbuf;
static u8_t *rxbuf0;
static u8_t *rxbuf1;

static int eth_initialize(struct device *dev)
{
    const struct eth_liteeth_config *config = dev->config->config_info;
    config->config_func();

    return 0;
}

static void eth_tx_data(struct eth_liteeth_dev_data *context, u8_t *data, u16_t len)
{
    /* copy data to slot */
    memcpy(txbuf, data, len);

    sys_write8(txslot, LITEETH_TX_SLOT);
    sys_write8(len >> 8, LITEETH_TX_LENGTH);
    sys_write8(len, LITEETH_TX_LENGTH + 4);

    /* wait for the device to be ready to transmit */
    while (!sys_read8(LITEETH_TX_READY));

    /* start transmitting */
    sys_write8(1, LITEETH_TX_START);

    /* change slot */
    txslot = (txslot + 1) % 2;
    txbuf = (txslot) ? txbuf1 : txbuf0;
}

static int eth_tx(struct device *dev, struct net_pkt *pkt)
{
    struct eth_liteeth_dev_data *context = dev->driver_data;
    unsigned int key;

    key = irq_lock();

    /* call eth_tx_data for each frame */
    for (struct net_buf *frag = pkt->frags; frag; frag = frag->frags)
        eth_tx_data(context, frag->data, frag->len);

    irq_unlock(key);

    net_pkt_unref(pkt);

    return 0;
}

static void eth_rx(struct device *port)
{
    struct net_pkt *pkt;
    struct eth_liteeth_dev_data *context = port->driver_data;
    unsigned int key, r;
    u16_t len = 0;

    key = irq_lock();

    /* get frame's length */
    for (int i = 0; i < 4; i++) {
        len <<= 8;
        len |= sys_read8(LITEETH_RX_LENGTH + i*0x4);
    }

    /* which slot is the frame in */
    rxslot = sys_read8(LITEETH_RX_SLOT);
    rxbuf = (rxslot) ? rxbuf1 : rxbuf0;

    /* obtain rx buffer */
    pkt = net_pkt_get_reserve_rx(K_NO_WAIT); 
    if (!pkt) {
        net_pkt_unref(pkt);
        irq_unlock(key);
        return;
    }

    /* copy data do buffer */
    if (!net_pkt_append_all(pkt, len, rxbuf, K_NO_WAIT)) {
        net_pkt_unref(pkt);
        irq_unlock(key);
        return;
    }

    /* receive data */
    r = net_recv_data(context->iface, pkt);
    if (r < 0) {
        net_pkt_unref(pkt);
    }

    irq_unlock(key);

}

static void eth_irq_handler(struct device *port)
{
    /* check sram reader events (tx) */
    if (sys_read8(LITEETH_TX_EV_PENDING) & LITEETH_EV_TX) {
        /* ack reader irq */
        sys_write8(LITEETH_EV_TX, LITEETH_TX_EV_PENDING);
    }

    /* check sram writer events (rx) */
    if (sys_read8(LITEETH_RX_EV_PENDING) & LITEETH_EV_RX) {
        eth_rx(port);
        
        /* ack writer irq */
        sys_write8(LITEETH_EV_RX, LITEETH_RX_EV_PENDING);
    }
}

#ifdef CONFIG_ETH_LITEETH_0

struct eth_liteeth_dev_data eth_0_data = {
    .mac_addr = {
        CONFIG_ETH_LITEETH_0_MAC_1,
        CONFIG_ETH_LITEETH_0_MAC_2,
        CONFIG_ETH_LITEETH_0_MAC_3,
        CONFIG_ETH_LITEETH_0_MAC_4,
        CONFIG_ETH_LITEETH_0_MAC_5,
        CONFIG_ETH_LITEETH_0_MAC_6
    }
};

static void eth0_irq_config(void);
struct eth_liteeth_config eth_0_config = {
    .config_func = eth0_irq_config,
};

static void eth_0_iface_init(struct net_if *iface)
{
    struct device *port = net_if_get_device(iface);
    struct eth_liteeth_dev_data *context = port->driver_data;
    static bool init_done;

    /* initialize only once */
    if (init_done)
        return;

    /* set interface */
    context->iface = iface;

    /* set MAC address */
    net_if_set_link_addr(iface, context->mac_addr, 6, NET_LINK_ETHERNET);

    /* clear pending events */
    sys_write8(LITEETH_EV_TX, LITEETH_TX_EV_PENDING);
    sys_write8(LITEETH_EV_RX, LITEETH_RX_EV_PENDING);

    /* setup tx slots */
    txslot = 0;
    txbuf0 = (u8_t *)LITEETH_SLOT_TX0;
    txbuf1 = (u8_t *)LITEETH_SLOT_TX1;

    /* setup rx slots */
    rxslot = 0;
    rxbuf0 = (u8_t *)LITEETH_SLOT_RX0;
    rxbuf1 = (u8_t *)LITEETH_SLOT_RX1;

    txbuf = txbuf0;
    rxbuf = rxbuf0;

    init_done = true;
}

static const struct ethernet_api eth_api = {
    .iface_api.init = eth_0_iface_init,
    .send = eth_tx
};

ETH_NET_DEVICE_INIT(
    eth0_liteeth,
    LITEETH_IF_NAME,
    eth_initialize,
    &eth_0_data,
    &eth_0_config,
    CONFIG_ETH_INIT_PRIORITY,
    &eth_api,
    1500    /* Ethernet MTU */
);


static void eth0_irq_config(void)
{
    IRQ_CONNECT(
        LITEETH_IRQ,CONFIG_ETH_LITEETH_0_IRQ_PRI,
        eth_irq_handler,
        DEVICE_GET(eth0_liteeth),
        0
    );
    irq_enable(LITEETH_IRQ);
    sys_write8(1, LITEETH_RX_EV_ENABLE);
}

#endif  /* CONFIG_ETH_LITEETH_0 */
