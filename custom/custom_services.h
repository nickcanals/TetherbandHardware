#ifndef _IDENTIFY_H
#define _IDENTIFY_H

#include <stdint.h>
#include <stdbool.h>
#include "ble.h"
#include "ble_srv_common.h"
#include "nrf_sdh_ble.h"


#define BLE_IDENTIFY_OBSERVER_PRIO 2

#define BLE_IDENTIFY_DEF(_name) \
    static ble_identify_t _name; \

#define IDENTIFY_UUID_BASE {0xca, 0xbf, 0xc9, 0x58 , 0xab, 0xc9, 0x21, 0x46, 0xf5, 0xa2, 0xbc, 0x97, 0x39, 0x1f, 0x20, 0xb0}
#define CONFIG_UUID_BASE {0xca, 0xbf, 0xc9, 0x58 , 0xab, 0xc9, 0x21, 0x46, 0xf5, 0xa2, 0xbc, 0x97, 0xf5, 0x46, 0x20, 0xb1}
#define IDENTIFY_UUID_SERVICE 0x1f39
#define CONFIG_UUID_CHAR 0x46F5

static uint8_t received_val;

typedef struct{
    uint8_t identifier;
    ble_srv_cccd_security_mode_t char_attr_md;
    void* evt_handler;
}ble_identify_init_t;

struct ble_identify_s{
    uint16_t service_handle;
    ble_gatts_char_handles_t value_handles;
    uint16_t conn_handle;
    uint8_t uuid_type;
    void* evt_handler;
};

typedef struct ble_identify_s ble_identify_t;

uint32_t ble_identify_init(ble_identify_t * p_identify, const ble_identify_init_t * p_identify_init); 

static ret_code_t bracelet_config_char_add(ble_identify_t * p_identify, const ble_identify_init_t * p_identify_init);

void ble_identify_on_evt(ble_identify_t * p_identify, ble_evt_t const * p_ble_evt);

static void on_write(ble_identify_t * p_identify, ble_evt_t const * p_ble_evt);



#endif
