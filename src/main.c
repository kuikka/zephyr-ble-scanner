/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Based on scan_adv and cdc_acm samples in Zephyr */

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <sys/printk.h>
#include <sys/util.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <sys/byteorder.h>

#include <usb/usb_device.h>
#include <sys/ring_buffer.h>
#include <drivers/uart.h>
#include <logging/log.h>

#include <stdio.h>

#include "conn.h"

LOG_MODULE_REGISTER(ble_scanner, LOG_LEVEL_INF);

RING_BUF_ITEM_DECLARE_POW2(uart_tx_ringbuf, 10);
K_MUTEX_DEFINE(uart_tx_ringbuf_mutex);

RING_BUF_ITEM_DECLARE_POW2(uart_rx_ringbuf, 10);
struct k_sem uart_rx_ringbuffer_semaphore;

static void interrupt_handler(struct device *dev)
{
	while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
		// Ignore input. Serve RX interrupt regardless.
		if (uart_irq_rx_ready(dev)) {
			uint8_t buffer[64];

			int bytes_read = uart_fifo_read(dev, buffer, sizeof(buffer));

			ring_buf_put(&uart_rx_ringbuf, buffer, bytes_read);
			k_sem_give(&uart_rx_ringbuffer_semaphore);
		}

		if (uart_irq_tx_ready(dev)) {
			uint8_t buffer[64];
			int rb_len, send_len;

			rb_len = ring_buf_get(&uart_tx_ringbuf, buffer, sizeof(buffer));
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
	int8_t rssi;
	// Type of advertisement
	uint8_t adv_type;
	// Advertisement data length
	uint8_t adv_data_len;
	// Advertisement data
	uint8_t adv_data[MAX_ADV_SIZE];
};

K_MEM_SLAB_DEFINE(adv_data_slab, sizeof(struct adv_data), 64, 4);
K_FIFO_DEFINE(adv_data_fifo);

static void scan_cb(const bt_addr_le_t *addr, int8_t rssi, uint8_t adv_type,
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

void start_scan()
{
	static struct bt_le_scan_param scan_param = {
		.type       = BT_HCI_LE_SCAN_PASSIVE,
		.filter_dup = BT_HCI_LE_SCAN_FILTER_DUP_DISABLE,
		.interval   = 0x0010,
		.window     = 0x0010,
	};

#if 1
	int err = bt_le_scan_start(&scan_param, scan_cb);
	if (err) {
		printk("Starting scanning failed (err %d)\n", err);
		return;
	}
#endif
}

void stop_scan()
{
	bt_le_scan_stop();
}

static struct bt_conn_cb conn_callbacks = {
		.connected = connected,
		.disconnected = disconnected,
};

#define MY_STACK_SIZE 500
#define MY_PRIORITY 5

void ble_connecting_thread(void *, void *, void *);

enum BleCommand
{
	BLE_CMD_CONNECT,
};

struct ble_data
{
	enum BleCommand cmd;
	bt_addr_le_t addr;
};

K_MEM_SLAB_DEFINE(ble_data_slab, sizeof(struct ble_data), 64, 4);
K_FIFO_DEFINE(ble_data_fifo);

K_THREAD_DEFINE(ble_connecting_thread_tid, MY_STACK_SIZE,
                ble_connecting_thread, NULL, NULL, NULL,
                MY_PRIORITY, 0, 0);

void ble_connecting_thread(void *unused1, void *unused2, void *unused3)
{
	while(1)
	{
		struct ble_data *data;
		data = k_fifo_get(&ble_data_fifo, K_FOREVER);

	}
}

static void uart_rx_thread(void *unused1, void *unused2, void *unused3)
{
	k_sem_init(&uart_rx_ringbuffer_semaphore, 0, 1000);

	char cmd_buf[128];
	int cmd_buf_idx = 0;

	while(1)
	{
		
		if (k_sem_take(&uart_rx_ringbuffer_semaphore, K_FOREVER) == 0)
		{
			uint8_t buffer[65];
			memset(buffer, 0, sizeof(buffer));
			uint32_t bytes_read = ring_buf_get(&uart_rx_ringbuf, buffer, sizeof(buffer) - 1);

			// if (bytes_read > 0) {
			// 	printk(buffer);
			// }

			int space_in_cmd_buf = sizeof(cmd_buf) - cmd_buf_idx;
			// Make space in command buffer
			if (space_in_cmd_buf < bytes_read)
			{
				const int bytes_to_shift = bytes_read - space_in_cmd_buf;
				memmove(&cmd_buf[0], &cmd_buf[bytes_to_shift], cmd_buf_idx - bytes_to_shift);
				cmd_buf_idx = cmd_buf_idx - bytes_to_shift;
			}

			space_in_cmd_buf = sizeof(cmd_buf) - cmd_buf_idx;
			const int bytes_to_copy = space_in_cmd_buf >= bytes_read ? bytes_read : space_in_cmd_buf;
			memcpy(&cmd_buf[cmd_buf_idx], buffer, bytes_to_copy);
			cmd_buf_idx += bytes_to_copy;

			cmd_buf[cmd_buf_idx] = 0;
			// printk("CMD_BUF: %s\n", cmd_buf);

			char *lf = strchr(cmd_buf, '\n');
			if (lf != NULL)
			{
				*lf = 0;

				char * comma = strchr(cmd_buf, ',');
				if (comma != NULL)
				{
					*comma = 0;
					char *cmd = cmd_buf;
					char *arg = comma + 1;
					printk("\nCMD: \"%s\", arg \"%s\"\n", cmd, arg);
					handle_command(cmd, arg);
					cmd_buf_idx = 0;
				}

			}
		}
	}
}

K_THREAD_DEFINE(uart_rx_thread_tid, MY_STACK_SIZE,
                uart_rx_thread, NULL, NULL, NULL,
                MY_PRIORITY, 0, 0);

static struct device *usb_uart;

void write_tx(const char *s)
{
	k_mutex_lock(&uart_tx_ringbuf_mutex, K_FOREVER);
	ring_buf_put(&uart_tx_ringbuf, s, strlen(s));
	k_mutex_unlock(&uart_tx_ringbuf_mutex);

	uart_irq_tx_enable(usb_uart);
}

void main(void)
{
	int err;

	printk("Starting BLE scanner\n");

	usb_uart = device_get_binding("CDC_ACM_0");
	if (!usb_uart) {
		LOG_ERR("CDC ACM device not found");
		return;
	}

	err = usb_enable(NULL);
	if (err != 0) {
		LOG_ERR("Failed to enable USB");
		return;
	}

	uart_irq_callback_set(usb_uart, interrupt_handler);

	/* Enable rx interrupts */
	uart_irq_rx_enable(usb_uart);


	/* Initialize the Bluetooth Subsystem */
	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	bt_conn_cb_register(&conn_callbacks);

	start_scan();

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

			snprintf(buf, sizeof(buf), "%s,%d,%d,%d,%d,%s\n", strbuf, data->addr.type, data->rssi, data->adv_type, data->adv_data_len, adv_data_hex);
			write_tx(buf);

			k_mem_slab_free(&adv_data_slab, (void**) &data);
		}
	} while (1);
}
