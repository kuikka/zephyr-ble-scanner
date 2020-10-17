#pragma once

void start_scan();
void stop_scan();

void connected(struct bt_conn *conn, uint8_t err);
void disconnected(struct bt_conn *conn, uint8_t reason);

void disconnect(struct bt_conn *conn);

void handle_command(const char *cmd, const char *arg);
void write_tx(const char *s);