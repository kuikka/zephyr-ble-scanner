#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <sys/printk.h>
#include <sys/util.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>

#include "sensor_scan.h"

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
	struct bt_le_scan_param scan_param = {
		.type       = BT_HCI_LE_SCAN_PASSIVE,
		.filter_dup = BT_HCI_LE_SCAN_FILTER_DUP_DISABLE,
		.interval   = 0x0010,
		.window     = 0x0010,
	};

	int err = bt_le_scan_start(&scan_param, scan_cb);
	if (err) {
		printk("Starting scanning failed (err %d)\n", err);
		return;
	}
}

void stop_scan()
{
	bt_le_scan_stop();
}

struct adv_data *get_adv_data()
{
    return k_fifo_get(&adv_data_fifo, K_FOREVER);
}

void free_adv_data(struct adv_data *adv)
{
    k_mem_slab_free(&adv_data_slab, (void**) &adv);
}
