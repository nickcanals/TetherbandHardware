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
#define CAPSENSE_UUID_BASE {0xca, 0xbf, 0xc9, 0x58 , 0xab, 0xc9, 0x21, 0x46, 0xf5, 0xa2, 0xbc, 0x97, 0xee, 0x29, 0x20, 0xb2}
#define IDENTIFY_UUID_SERVICE 0x1f39
#define CONFIG_UUID_CHAR 0x46F5
#define CAPSENSE_UUID_CHAR 0x29EE

#define BACKGROUND_DISTANCE_NOTIFICATION 1
#define CAPSENSE_BRACELET_ON_NOTIFICATION 2
#define CAPSENSE_BRACELET_REMOVED_NOTIFICATION 3

#define TRACKING_STARTED       1

// Priorities for local alert events sent from app, higher numbers are higher priorities
#define STOP_EMERGENCY_ALERT   8
#define START_EMERGENCY_ALERT  7
#define IN_RANGE               6
#define OUT_OF_RANGE           5
#define BRACELET_ON            4
#define BRACELET_REMOVED       3

#define BACKGROUND_DISTANCE_TIMER_INTERVAL 12000 // When iPhone app is backgrounded, config characteristic sends notifications every 12 seconds to trigger iphone to read RSSI in background.

#define NUM_CHARACTERISTICS 2

static uint8_t received_val;

typedef struct ble_identify_s ble_identify_t;

typedef void (* ble_identify_evt_handler_t) (uint8_t event_flag);

/*typedef struct
{
    uint8_t evt_type;    
    uint16_t conn_handle; 
} ble_bas_evt_t;*/

typedef struct{
    uint8_t identifier;
    ble_srv_cccd_security_mode_t char_attr_md;
    ble_identify_evt_handler_t evt_handler;
}ble_identify_init_t;

struct ble_identify_s{
    uint16_t service_handle;
    ble_gatts_char_handles_t value_handles[NUM_CHARACTERISTICS];
    uint16_t conn_handle;
    uint8_t uuid_type;
    ble_identify_evt_handler_t evt_handler;
};

uint32_t ble_identify_init(ble_identify_t * p_identify, const ble_identify_init_t * p_identify_init); 

static ret_code_t bracelet_char_add(ble_identify_t * p_identify, const ble_identify_init_t * p_identify_init, ble_uuid128_t char_base_uuid, uint16_t char_uuid, uint8_t char_index);

void ble_identify_on_evt(ble_identify_t * p_identify, ble_evt_t const * p_ble_evt);

static void on_write(ble_identify_t * p_identify, ble_evt_t const * p_ble_evt);

ret_code_t ble_identify_update(ble_identify_t * p_identify, uint16_t conn_handle, uint8_t type);

static ret_code_t background_distance_notification_send(ble_gatts_hvx_params_t * const p_hvx_params, uint16_t conn_handle);

ret_code_t add_background_distance_timer(void * timeout_handler);

#endif
