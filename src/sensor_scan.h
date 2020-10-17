#pragma once

#include <stdint.h>

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

void start_scan();
void stop_scan();

struct adv_data *get_adv_data();
void free_adv_data(struct adv_data *adv);