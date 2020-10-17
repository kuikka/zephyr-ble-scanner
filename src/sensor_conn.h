#pragma once

void disconnect(struct bt_conn *conn);

void handle_command(const char *cmd, const char *arg);
void write_tx(const char *s);

void sensor_conn_init();