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

static struct bt_conn *default_conn;

enum DeviceTypes
{
    LYWSD03MMC,
};
static enum DeviceTypes current_device_type;

#if 0

static void start_discovery(uint8_t type);
static struct bt_gatt_discover_params discover_params;
static uint8_t current_discovery_type;

static uint8_t discover_func(struct bt_conn *conn,
			     const struct bt_gatt_attr *attr,
			     struct bt_gatt_discover_params *params)
{
	if (!attr) {
		printk("Discover complete\n");

        if (current_discovery_type == BT_GATT_DISCOVER_PRIMARY)
        {
            start_discovery(BT_GATT_DISCOVER_CHARACTERISTIC);
        }
        else if (current_discovery_type == BT_GATT_DISCOVER_CHARACTERISTIC)
        {
            start_discovery(BT_GATT_DISCOVER_DESCRIPTOR);
        }
        else
        {
            (void)memset(params, 0, sizeof(*params));
            bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        }

		return BT_GATT_ITER_STOP;
	}

	if (bt_uuid_cmp(attr->uuid, BT_UUID_GATT_PRIMARY) == 0)
	{
		printk("Primary: ");
	}
	else if (bt_uuid_cmp(attr->uuid, BT_UUID_GATT_CHRC) == 0)
	{
		printk("Characteristic: ");
	}
	else if (bt_uuid_cmp(attr->uuid, BT_UUID_GATT_CCC) == 0)
	{
		printk("CCC: ");
	}
	
	printk("[ATTRIBUTE] handle %x\n", attr->handle);

	return BT_GATT_ITER_CONTINUE;
}

static void start_discovery(uint8_t type)
{
	discover_params.uuid = NULL, // &uuid.uuid;
	discover_params.func = discover_func;
	discover_params.start_handle = 0x0001;
	discover_params.end_handle = 0xffff;
	discover_params.type = type;

    current_discovery_type = type;

	int err = bt_gatt_discover(default_conn, &discover_params);
	if (err) {
		printk("Discover failed(err %d)\n", err);
		return;
	}
}
#endif

static uint8_t notify_func_lywsd03mmc(struct bt_conn *conn,
			   struct bt_gatt_subscribe_params *params,
			   const void *data, uint16_t length)
{
	if (!data) {
		printk("[UNSUBSCRIBED]\n");
		params->value_handle = 0U;
        disconnect(conn);
		return BT_GATT_ITER_CONTINUE;
	}

    printk("Notified, length = %d\n", length);

    const uint8_t *d = data;
    for (int i = 0; i < length; i++)
    {
        printk("%d = %x\n", i, d[i]);
    }


    int16_t t = d[0] | (d[1] << 8);
    uint16_t humidity = d[2];
    uint16_t voltage = d[3] | (d[4] << 8);

    char addr[BT_ADDR_LE_STR_LEN];
    char buffer[64];

    bt_addr_to_str(&(bt_conn_get_dst(conn)->a), addr, sizeof(addr));

    printk("Temperature: %d, humidity %u, battery voltage %u\n", t, humidity, voltage);

    snprintf(buffer, sizeof(buffer), "%s,lywsd03mmc,%d,%u,%u\n", addr, t, humidity, voltage);
    write_tx(buffer);

    bt_gatt_unsubscribe(conn, params);

    disconnect(conn);

	return BT_GATT_ITER_STOP;
}

static void write_func_lywsd03mmc(struct bt_conn *conn, uint8_t err,
				     struct bt_gatt_write_params *params)
{
    printk("CMD written! err=%d\n", err);

    if (err)
    {
        disconnect(conn);
    }
}

void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err) {
		printk("Failed to connect to %s (%d)\n", addr, err);

		bt_conn_unref(default_conn);
		default_conn = NULL;

		start_scan();
		return;
	}

	printk("Connected: %s\n", addr);

    // start_discovery(BT_GATT_DISCOVER_PRIMARY);
    // start_discovery(BT_GATT_DISCOVER_ATTRIBUTE);

    if (current_device_type == LYWSD03MMC)
    {
        static uint8_t data[3] = {
            0xf4, 0x01, 0x00,
        };

        static struct bt_gatt_write_params write_params = {
            .data = data,
            .length = sizeof(data),
            .handle = 0x0046,
            .func = write_func_lywsd03mmc,
        };
        err = bt_gatt_write(conn, &write_params);
        if (err) {
            printk("bt_gatt_write failed %d", err);
            disconnect(conn);
        }

        static struct bt_gatt_subscribe_params sub_params = {
            .notify = notify_func_lywsd03mmc,
            .value = BT_GATT_CCC_INDICATE,
            .value_handle = 0x0036,
            .ccc_handle = 0x0038,
        };

        err = bt_gatt_subscribe(conn, &sub_params);
        if (err && err != -EALREADY) {
            printk("Subscribe failed (err %d)\n", err);
            disconnect(conn);
        } else {
            printk("[SUBSCRIBED]\n");
        }
    }
}

void disconnect(struct bt_conn *conn)
{
    bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
}

void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	if (conn != default_conn) {
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Disconnected: %s (reason 0x%02x)\n", addr, reason);

	bt_conn_unref(default_conn);
	default_conn = NULL;

	start_scan();
}

void handle_command(const char *cmd, const char *arg)
{
	if (strcmp(cmd, "LYWSD03MMC") == 0)
	{
		bt_addr_le_t addr;
		bt_addr_from_str(arg, &addr.a);
		addr.type = BT_ADDR_LE_PUBLIC;

		stop_scan();

		struct bt_conn_le_create_param create_param = {
			.options = BT_CONN_LE_OPT_NONE,
			.interval = BT_GAP_SCAN_FAST_INTERVAL,
			.window = BT_GAP_SCAN_FAST_INTERVAL,
			.timeout = 1000, // 10s
		};

		struct bt_le_conn_param conn_param = {
			.interval_min = BT_GAP_INIT_CONN_INT_MIN,
			.interval_max = BT_GAP_INIT_CONN_INT_MAX,
			.latency = 0,
			.timeout = 400,
		};

        current_device_type = LYWSD03MMC;

		int err = bt_conn_le_create(&addr, &create_param,
				&conn_param, &default_conn);

		if (err) {
			printk("Create conn to %s failed (%d)\n", arg, err);
			start_scan();
            return;
		}

	}
}

