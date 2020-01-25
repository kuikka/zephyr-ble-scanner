/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Based on scan_adv and cdc_acm samples in Zephyr */

#include <zephyr/types.h>
#include <stddef.h>
#include <sys/printk.h>
#include <sys/util.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>

#include <usb/usb_device.h>
#include <sys/ring_buffer.h>
#include <drivers/uart.h>
#include <logging/log.h>

#include <stdio.h>

LOG_MODULE_REGISTER(ble_scanner, LOG_LEVEL_INF);

RING_BUF_ITEM_DECLARE_POW2(ringbuf, 10);

static void interrupt_handler(struct device *dev)
{
	while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
		// Ignore input. Serve RX interrupt regardless.
		if (uart_irq_rx_ready(dev)) {
			u8_t buffer[64];

			uart_fifo_read(dev, buffer, sizeof(buffer));
		}

		if (uart_irq_tx_ready(dev)) {
			u8_t buffer[64];
			int rb_len, send_len;

			rb_len = ring_buf_get(&ringbuf, buffer, sizeof(buffer));
			if (!rb_len) {
				LOG_DBG("Ring buffer empty, disable TX IRQ");
				uart_irq_tx_disable(dev);
				continue;
			}

			send_len = uart_fifo_fill(dev, buffer, rb_len);
			if (send_len < rb_len) {
				LOG_ERR("Drop %d bytes", rb_len - send_len);
			}
		}
	}
}

#define MAX_ADV_SIZE	(32)

struct adv_data
{
	// Fifo pointer, used by K_FIFO
	void *fifo_ptr;

	// Sender's address
	bt_addr_le_t addr;
	s8_t rssi;
	// Type of advertisement
	u8_t adv_type;
	// Advertisement data length
	uint8_t adv_data_len;
	// Advertisement data
	uint8_t adv_data[MAX_ADV_SIZE];
};

K_MEM_SLAB_DEFINE(adv_data_slab, sizeof(struct adv_data), 64, 4);
K_FIFO_DEFINE(adv_data_fifo);

static void scan_cb(const bt_addr_le_t *addr, s8_t rssi, u8_t adv_type,
		    struct net_buf_simple *buf)
{
	struct adv_data *data;

	if (k_mem_slab_alloc(&adv_data_slab, (void**) &data, K_NO_WAIT) == 0)
	{
		data->addr = *addr;
		data->rssi = rssi;
		data->adv_type = adv_type;
		data->adv_data_len = MIN(buf->len, sizeof(data->adv_data));
		memcpy(data->adv_data, buf->data, data->adv_data_len);

		k_fifo_put(&adv_data_fifo, data);
	}
	else
	{
		//printk("Could not allocate fifo item!\r\n");
	}
}

void main(void)
{
	struct device *dev;
	int err;

	printk("Starting BLE scanner\n");

	dev = device_get_binding("CDC_ACM_0");
	if (!dev) {
		LOG_ERR("CDC ACM device not found");
		return;
	}

	err = usb_enable(NULL);
	if (err != 0) {
		LOG_ERR("Failed to enable USB");
		return;
	}

	uart_irq_callback_set(dev, interrupt_handler);

	/* Enable rx interrupts */
	uart_irq_rx_enable(dev);


	/* Initialize the Bluetooth Subsystem */
	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	struct bt_le_scan_param scan_param = {
		.type       = BT_HCI_LE_SCAN_PASSIVE,
		.filter_dup = BT_HCI_LE_SCAN_FILTER_DUP_DISABLE,
		.interval   = 0x0010,
		.window     = 0x0010,
	};

	err = bt_le_scan_start(&scan_param, scan_cb);
	if (err) {
		printk("Starting scanning failed (err %d)\n", err);
		return;
	}

	do {
		struct adv_data *data;

		data = k_fifo_get(&adv_data_fifo, K_FOREVER);

		if (data != NULL)
		{
			static char strbuf[32];
			static char adv_data_hex[MAX_ADV_SIZE * 2 + 1];
			static char buf[1024];

			bt_addr_to_str(&data->addr.a, strbuf, sizeof(strbuf));

			bin2hex(data->adv_data, data->adv_data_len, adv_data_hex, sizeof(adv_data_hex));

			snprintf(buf, sizeof(buf), "%s,%d,%d,%d,%d,%s\r\n", strbuf, data->addr.type, data->rssi, data->adv_type, data->adv_data_len, adv_data_hex);
			ring_buf_put(&ringbuf, buf, strlen(buf));

			uart_irq_tx_enable(dev);

			k_mem_slab_free(&adv_data_slab, (void**) &data);
		}
	} while (1);
}
