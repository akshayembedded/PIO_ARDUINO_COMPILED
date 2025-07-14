#ifndef LUAT_ESPNOW_H
#define LUAT_ESPNOW_H

#include "luat_base.h"
#ifdef __LUATOS__
#include "luat_msgbus.h"
#include "luat_mem.h"
#endif


#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Callback type for receiving ESP-NOW data
typedef void (*luat_espnow_recv_cb_t)(const uint8_t *mac_addr, const uint8_t *data, size_t len);

// Initialize ESP-NOW
bool luat_espnow_init(void);

// Deinitialize ESP-NOW (optional)
void luat_espnow_deinit(void);

// Send data to a peer (mac[6])
bool luat_espnow_send(const uint8_t *mac_addr, const uint8_t *data, size_t len);

// Add a peer to the peer list
bool luat_espnow_add_peer(const uint8_t *mac_addr);

// Remove a peer
bool luat_espnow_del_peer(const uint8_t *mac_addr);

// Set receive callback
void luat_espnow_set_recv_cb(luat_espnow_recv_cb_t cb, lua_State* L);

// Clear receive callback
void luat_espnow_clear_recv_cb(void);


#ifdef __cplusplus
}
#endif


#endif